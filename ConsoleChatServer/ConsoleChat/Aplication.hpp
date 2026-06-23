#include "ENetServer.hpp"
#include <iostream>
#include <thread>
#include <sodium.h>
#include <ThreadSafeQueue.hpp>
#include "common.hpp"
#include "exceptions.hpp"
#include "DB.hpp"
#include "BiMap.hpp"
struct OutPacket {
    mpv::Vector<size_t> targets;
    ENetPacket* packet;
    enet_uint8 channel = 0;
};
struct InPacket {// Mas tarde tengo que ver quien va a eliminar estos paquetes
    size_t sender;
    ENetPacket* packet;
    InPacket(const InPacket&) = delete;
    InPacket& operator=(const InPacket&) = delete;
    InPacket(const size_t sender,ENetPacket*const packet):sender(sender),packet(packet){}
    InPacket(InPacket&& other) :sender(other.sender), packet(other.packet) {
        other.packet = nullptr;
    }
    InPacket& operator=(InPacket&& other) {
        sender = other.sender;
        packet = other.packet;
        other.packet = nullptr;
        return *this;
    }
    ~InPacket() {
        if(packet) enet_packet_destroy(packet);
    }
};
class Aplication {
	Server server;
    mpv::Database users = mpv::Database(L"database.txt");
    mpv::BiMap<size_t, mpv::String> connected_users;
    mpv::Map<size_t, ENetPeer*> id_to_peer;
    size_t next_clientID=0;
    std::thread network_thread, logic_thread;
    std::mutex mtx;
    mpv::tsQueue<InPacket> incoming_events;
    mpv::tsQueue<OutPacket> outgoing_events;
    std::atomic<bool> running = false;
    using MapIterator = mpv::Map<size_t, ENetPeer*>::iterator;
    using WrappedStr = mpv::serialization::Wrapper<mpv::String, unsigned char>;

    void network_loop() {
        while (running) {
            while (server.host_service(1000) > 0) { // Este bucle encola paquetes entrantes
                switch (ENetPeer* peer = server.get_event().peer; server.get_event().type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "A new client connected from " << peer->address.host << ":" << peer->address.port << ".\n";
                    *reinterpret_cast<MapIterator*>(&peer->data) = id_to_peer.insert({ next_clientID++,peer });
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    puts("Packet recieved");
                    incoming_events.push({ getID(peer),server.get_event().packet });
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << peer->address.host << ":" << peer->address.port << " disconnected.\n";
                    incoming_events.push({ getID(peer),nullptr }); // packet = nullptr indicates the logic thread that the peer has been disconnected, so it should delete the user from connected_users.
                    id_to_peer.del(*reinterpret_cast<MapIterator*>(&peer->data));
                    break;
                }
            }
            mpv::Optional<OutPacket> outpacket;
            while (outgoing_events.try_pop(outpacket)) { // Consume paquetes salientes (envia)
                for (size_t targetID : outpacket->targets) {
                    if (mpv::Optional<ENetPeer*> peer = id_to_peer.get(targetID)) {
                        enet_peer_send(*peer, outpacket->channel, outpacket->packet);
                    }
                }
                std::cout << "Packet sent to: " << outpacket->targets << std::endl;
            }
        }
    }
    void logic_loop() {
        while (running) {
            mpv::Optional<InPacket> inpacket;
            while (incoming_events.try_pop(inpacket)) { // Consume paquetes entrantes (procesa)
                if (inpacket->packet == nullptr) // si es solo una desconeccion
                    connected_users.erase_by_left(inpacket->sender);
                else // si es un paquete
                    process_packet(mpv::move(*inpacket));
            }
        }
    }
    void process_packet(InPacket inpacket) {
        try {
            if (inpacket.packet->dataLength < sizeof(ServerOpcode)) throw broken_packet();
            switch (*reinterpret_cast<ServerOpcode*>(inpacket.packet->data)) {
            case ADD_USER: add_user(inpacket);break;
            case DELETE_USER: delete_user(inpacket);break;
            case LOG_IN: log_in(inpacket);break;
            case LOG_OUT: log_out(inpacket);break;
            case SEND_MESSAGE: send_message(inpacket);break;
            case SEND_COMMAND: send_command(inpacket);break;
            default: throw broken_packet();
            }
        }
        catch (const std::exception& ex) {
            puts(ex.what());
        }
    }
    void push_success(size_t id, ClientOpcode op) {
        outgoing_events.push({ {id},enet_packet_create(&op,sizeof(ClientOpcode),ENET_PACKET_FLAG_RELIABLE) });
    }
    void push_failure(size_t id, ClientOpcode op, const std::exception& e) {
        size_t size = strlen(e.what());
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[sizeof(ClientOpcode) + size]{ op });
        mpv::copy_n(data.get() + sizeof(ClientOpcode), e.what(), size);
        outgoing_events.push({ {id},enet_packet_create(data.get(),size,ENET_PACKET_FLAG_RELIABLE) });
    }
    void add_user(const InPacket& inpacket) {
        try {
            constexpr size_t expected_header_size = sizeof(ServerOpcode) + 2;
            if (inpacket.packet->dataLength < expected_header_size) throw broken_packet();
            enet_uint8* data = inpacket.packet->data + sizeof(ServerOpcode);
            enet_uint8 username_size = *(data++);
            enet_uint8 password_size = *(data++);
            if (inpacket.packet->dataLength < expected_header_size + username_size + password_size) throw broken_packet();
            mpv::String username((char*)data, username_size);
            data += username_size;
            mpv::String password((char*)data, password_size);
            mpv::String hashed_password((size_t)crypto_pwhash_STRBYTES - 1);
            if (!is_valid_username(username)) throw invalid_username();
            if (users.get_dict().contains(username)) throw existing_user(username);
            if (int error = crypto_pwhash_str(hashed_password.c_str(), password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) throw hasher_exception(error);
            users.insert(username, hashed_password);
            push_success(inpacket.sender, SIGNIN_SUCCESS);
            std::cout << "User created <" << username << ">\n";
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.sender, SIGNIN_ERROR, ex);
        }
    }
    void log_in(const InPacket& inpacket) {
        try {
            constexpr size_t expected_header_size = sizeof(ServerOpcode) + 2;
            if (inpacket.packet->dataLength < expected_header_size) throw broken_packet();
            enet_uint8* data = inpacket.packet->data + sizeof(ServerOpcode);
            enet_uint8 username_size = *(data++);
            enet_uint8 password_size = *(data++);
            if (inpacket.packet->dataLength < expected_header_size + username_size + password_size) throw broken_packet();
            mpv::String username((char*)data, username_size);
            data += username_size;
            mpv::String password((char*)data, password_size);
            
            if(connected_users.left().contains(inpacket.sender)) throw peer_already_has_user_associated();
            if (!is_valid_username(username)) throw invalid_username();
            if (!users.get_dict().contains(username)) throw not_existing_user(username);
            if (connected_users.right().contains(username)) throw already_connected_user(username);
            if (crypto_pwhash_str_verify(users.get_dict().at(username).c_str(), password.c_str(), password.size()) != 0) throw wrong_password();
            
            connected_users.insert(inpacket.sender,username);
            push_success(inpacket.sender, LOGIN_SUCCESS);
            std::cout << "User logged <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.sender, LOGIN_ERROR, ex);
        }
    }
    void log_out(const InPacket& inpacket) {
        try {
            if (inpacket.packet->dataLength > sizeof(ServerOpcode)) throw broken_packet(); // Delete user request only contains the opcode.
            auto it = connected_users.left().find(inpacket.sender);
            if (it == connected_users.left().end()) throw not_logged_in();
            mpv::String username = it->val;
            connected_users.erase_by_left(inpacket.sender);
            push_success(inpacket.sender, LOGOUT_SUCCESS);
            std::cout << "User logged out <" << username << ">\n";
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.sender, LOGOUT_ERROR, ex);
        }
    }
    void delete_user(const InPacket& inpacket) {
        try {
            if (inpacket.packet->dataLength > sizeof(ServerOpcode)) throw broken_packet(); // Delete user request only contains the opcode.
            auto it = connected_users.left().find(inpacket.sender);
            if (it == connected_users.left().end()) throw not_logged_in();
            mpv::String username = it->val;
            connected_users.erase_by_left(inpacket.sender);
            users.pop(username);
            push_success(inpacket.sender, DELETE_USER_SUCCESS);
            std::cout << "User deleted <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.sender, DELETE_USER_ERROR, ex);
        }
    }
    void send_message(const InPacket& inpacket) {
        try {
            const enet_uint8* end = inpacket.packet->data + inpacket.packet->dataLength;
            if (!connected_users.left().contains(inpacket.sender)) throw not_logged_in();
            const enet_uint8* ptr = inpacket.packet->data + sizeof(ServerOpcode);
            if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
            mpv::String destinatary((char*)ptr + 1, *ptr);
            ptr += sizeof(enet_uint8) + destinatary.size();
            mpv::String message((char*)ptr, inpacket.packet->dataLength - destinatary.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
            if (destinatary.size() == 0) send_general_message(inpacket.sender, message);
            else send_direct_message(inpacket.sender, destinatary, message);
            push_success(inpacket.sender, SUBMIT_SUCCESS);
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.sender, SUBMIT_ERROR, ex);
        }
    }
    void send_general_message(size_t sender, const mpv::String& message) {
        const mpv::String& sender_user = connected_users.left().at(sender);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + message.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ GENERAL_MESSAGE,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), message.c_str(), message.size());
        outgoing_events.push({ mpv::Vector<size_t>(connected_users.left().keys().begin(), connected_users.left().keys().end()),enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE) });
        std::cout << "message(global)<" << sender_user << ">: " << message << std::endl;
    }
    void send_direct_message(size_t sender, const mpv::String& destinatary, const mpv::String& message) {
        mpv::Optional<size_t> target = connected_users.right().get(destinatary);
        if (!target.has_value()) throw target_user_not_found();
        const mpv::String& sender_user = connected_users.left().at(sender);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + message.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ DIRECT_MESSAGE,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), message.c_str(), message.size());
        outgoing_events.push({ {*target},enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE) });
        std::cout << "message from <" << sender_user << "> to <" << destinatary << ">: " << message << std::endl;
    }
    void send_command(const InPacket& inpacket) {
        try {
            const enet_uint8* end = inpacket.packet->data + inpacket.packet->dataLength;
            if (!connected_users.left().contains(inpacket.sender)) throw not_logged_in();
            const enet_uint8* ptr = inpacket.packet->data + sizeof(ServerOpcode);
            if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
            mpv::String destinatary((char*)ptr + 1, *ptr);
            ptr += sizeof(enet_uint8) + destinatary.size();
            mpv::String command((char*)ptr, inpacket.packet->dataLength - destinatary.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
            if (destinatary.size() == 0) send_general_command(inpacket.sender, command);
            else send_direct_command(inpacket.sender, destinatary, command);
            push_success(inpacket.sender, SUBMIT_SUCCESS);
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.sender, SUBMIT_ERROR, ex);
        }
    }
    void send_general_command(size_t sender, const mpv::String& command) {
        const mpv::String& sender_user = connected_users.left().at(sender);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
        mpv::Vector<size_t> targets(connected_users.left().keys().begin(), connected_users.left().keys().end());
        targets.remove(sender);
        outgoing_events.push({ mpv::move(targets),enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE)});
        std::cout << "command(global)<" << sender_user << ">: " << command << std::endl;
    }
    void send_direct_command(size_t sender, const mpv::String& destinatary, const mpv::String& command) {
        mpv::Optional<size_t> target = connected_users.right().get(destinatary);
        if (!target.has_value()) throw target_user_not_found();
        const mpv::String& sender_user = connected_users.left().at(sender);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
        outgoing_events.push({ {*target},enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE) });
        std::cout << "command from <" << sender_user << "> to <" << destinatary << ">: " << command << std::endl;
    }
    //void send_general_command(const mpv::String& command) {
    //    ENetPeer* sender_peer = server.get_event().peer;
    //    const mpv::String& sender_user = (*reinterpret_cast<MapIterator*>(&sender_peer->data))->key;
    //    size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
    //    mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
    //    mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
    //    mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
    //    ENetPacket* packet = enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE);
    //    for (ENetPeer* peer : connected_users.values()) {
    //        if (peer != sender_peer) enet_peer_send(peer, 0, packet);
    //    }
    //    std::cout << "command(global)<" << sender_user << ">: " << command << std::endl;
    //}
    //void send_direct_command(const mpv::String& destinatary, const mpv::String& command) {
    //    mpv::Optional<ENetPeer*> peer = connected_users.get(destinatary);
    //    if (!peer.has_value()) throw target_user_not_found();
    //    ENetPeer* sender_peer = server.get_event().peer;
    //    const mpv::String& sender_user = (*reinterpret_cast<MapIterator*>(&sender_peer->data))->key;
    //    size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
    //    mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
    //    mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
    //    mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
    //    ENetPacket* packet = enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE);
    //    enet_peer_send(*peer, 0, packet);
    //    std::cout << "command from <" << sender_user << "> to <" << destinatary << ">: " << command << std::endl;
    //}
    //void send_command() {
    //    try {
    //        const enet_uint8* end = server.get_event().packet->data + server.get_event().packet->dataLength;
    //        if (*reinterpret_cast<MapIterator*>(&server.get_event().peer->data) == connected_users.end()) throw not_logged_in();
    //        const enet_uint8* ptr = server.get_event().packet->data + sizeof(ServerOpcode);
    //        if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
    //        mpv::String destinatary((char*)ptr + 1, *ptr);
    //        ptr += sizeof(enet_uint8) + destinatary.size();
    //        mpv::String command((char*)ptr, server.get_event().packet->dataLength - destinatary.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
    //        if (destinatary.size() == 0) send_general_command(command);
    //        else send_direct_command(destinatary, command);
    //        send_success(server.get_event().peer, SUBMIT_SUCCESS);
    //    }
    //    catch (const std::exception& ex) {
    //        puts(ex.what());
    //        send_failure(server.get_event().peer, SUBMIT_ERROR, ex);
    //    }
    //}

    /*
    send message()
        procesar mensaje(destinatario,mensaje)
        si destinatario vacio
            enviar a todos mensaje(usuario que lo envia,mensaje)
        sino
            enviar a destinatario mensaje(usuario que lo envia,mensaje)

    */

public:
	Aplication() : server(SERVER_PORT, 40) {
		if (server.get_host() == nullptr) {
			std::cerr << "An error occurred while creating an ENet server host\n";
			exit(EXIT_FAILURE);
		}
	}
	void start() {
        running = true;
        network_thread = std::thread(&Aplication::network_loop, this);
        logic_thread = std::thread(&Aplication::logic_loop, this);
	}
    void stop() {
        running = false;
        incoming_events.close();
        outgoing_events.close();
        if (network_thread.joinable()) network_thread.join();
        if (logic_thread.joinable()) logic_thread.join();
    }
};