#include "ENetServer.hpp"
#include <iostream>
#include <thread>
#include <sodium.h>
#include <ThreadSafeQueue.hpp>
#include <common.hpp>
#include "exceptions.hpp"
#include "DB.hpp"
#include "BiMap.hpp"
struct OutPacket {
    mpv::Vector<size_t> targets;
    ENetPacket* packet;
    enet_uint8 channel = 0;
};
struct InPacket {
    size_t senderID;
    ENetPacket* packet;
    InPacket(const InPacket&) = delete;
    InPacket& operator=(const InPacket&) = delete;
    InPacket(const size_t senderID, ENetPacket*const packet, enet_uint8 channel=0):senderID(senderID),packet(packet){}
    InPacket(InPacket&& other) :senderID(other.senderID), packet(other.packet){
        other.packet = nullptr;
    }
    InPacket& operator=(InPacket&& other) {
        senderID = other.senderID;
        packet = other.packet;
        other.packet = nullptr;
        return *this;
    }
    ENetPacket* drop() {
        ENetPacket* temp = packet;
        packet = nullptr;
        return temp;
    }
    ~InPacket() {
        if(packet) enet_packet_destroy(packet);
    }
};
struct TransferenceInfo {
    size_t transferID, senderID, targetID;
};
class Aplication {
	Server server;
    mpv::Database users = mpv::Database(L"database.txt");
    mpv::BiMap<size_t, mpv::String> connected_users;
    mpv::Map<size_t, ENetPeer*> id_to_peer;
    mpv::Map<size_t, mpv::Pair<size_t, size_t>> active_transfers;
    size_t next_clientID = 0, next_transferID = 0;
    std::thread network_thread, logic_thread, file_relay_thread;
    std::mutex mtx;
    mpv::tsQueue<InPacket> incoming_events;
    mpv::tsQueue<OutPacket> outgoing_events;
    mpv::tsQueue<InPacket> file_relay_queue;
    mpv::tsQueue<TransferenceInfo> file_transfers_queue;
    std::atomic<bool> running = false;
    using MapIterator = mpv::Map<size_t, ENetPeer*>::iterator;
    using WrappedStr = mpv::serialization::Wrapper<mpv::String, unsigned char>;

    void network_loop() {
        while (running) {
            while (server.host_service(0) > 0) { // Este bucle encola paquetes entrantes
                switch (ENetPeer* peer = server.get_event().peer; server.get_event().type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "A new client connected from " << peer->address.host << ":" << peer->address.port << ".\n";
                    *reinterpret_cast<MapIterator*>(&peer->data) = id_to_peer.insert({ next_clientID++,peer });
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    switch (server.get_event().channelID) {
                    case 0:incoming_events.push({ getID(peer), server.get_event().packet });break;
                    case 1:file_relay_queue.push({ getID(peer), server.get_event().packet });break;
                    }
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
            }
        }
    }
    void logic_loop() {
        while (running) {
            mpv::Optional<InPacket> inpacket;
            while (incoming_events.try_pop(inpacket)) { // Consume paquetes entrantes (procesa)
                if (inpacket->packet == nullptr) // si es solo una desconeccion
                    connected_users.erase_by_left(inpacket->senderID);
                else // si es un paquete
                    process_packet(mpv::move(*inpacket));
            }
        }
    }
    void transfer_router_loop() {
        while (running) {
            mpv::Optional<TransferenceInfo> transference;
            while (file_transfers_queue.try_pop(transference)) {
                active_transfers[transference->transferID] = { transference->senderID,transference->targetID };
                std::cout << "Active transfers: " << active_transfers << std::endl;
            }
            mpv::Optional<InPacket> inpacket;
            while (file_relay_queue.try_pop(inpacket)) {
                try {
                    if (inpacket->packet->dataLength < sizeof(ClientOpcode) + sizeof(size_t) + sizeof(bool) || *reinterpret_cast<ClientOpcode*>(inpacket->packet->data) != FILE_CHUNK) throw broken_packet();
                    const Chunk& chunk = *reinterpret_cast<const Chunk*>(inpacket->packet->data);
                    auto IDs = active_transfers.find(chunk.transferID);
                    if (IDs == active_transfers.end()) throw transferID_not_found();
                    if (inpacket->senderID != IDs->val.x1) throw senderID_mismatch();
                    outgoing_events.push({ {IDs->val.x2},inpacket->drop(),1 });
                    if (chunk.is_last_one) active_transfers.del(IDs);
                }
                catch (const std::exception& ex) {
                    puts(ex.what());
                }
            }
        }
    }
    void process_packet(InPacket inpacket) {
        try {
            if (inpacket.packet->dataLength < sizeof(enet_uint8)) throw broken_packet();
            switch (*reinterpret_cast<ServerOpcode*>(inpacket.packet->data)) {
            case ADD_USER: add_user(inpacket);break;
            case DELETE_USER: delete_user(inpacket);break;
            case LOG_IN: log_in(inpacket);break;
            case LOG_OUT: log_out(inpacket);break;
            case SEND_MESSAGE: send_message(inpacket);break;
            case SEND_COMMAND: send_command(inpacket);break;
            case FILE_HEADER: send_file_header(inpacket);break;
            default: throw broken_packet();
            }
        }
        catch (const std::exception& ex) {
            puts(ex.what());
        }
    }
    void send_file_header(const InPacket& inpacket) {
        size_t client_temp_fileID;
        try {
            mpv::serialization::Deserializer<broken_packet> deserializer(inpacket.packet->data + sizeof(ServerOpcode), inpacket.packet->dataLength - sizeof(ServerOpcode));
            client_temp_fileID = deserializer.read<size_t>();
            FileHeader fh = deserializer.read<FileHeader>();
            mpv::Optional<size_t> targetID = connected_users.right().get(fh.target);
            if (!connected_users.left().contains(inpacket.senderID)) throw not_logged_in();
            if (!targetID.has_value()) throw target_user_not_found();
            file_transfers_queue.push({ next_transferID,inpacket.senderID,*targetID });
            {
                enet_uint8 data[sizeof(ClientOpcode) + 2 * sizeof(size_t)] = { TRANSFERENCE_ID };
                *reinterpret_cast<size_t*>(data + sizeof(ClientOpcode)) = client_temp_fileID;
                *reinterpret_cast<size_t*>(data + sizeof(ClientOpcode) + sizeof(size_t)) = next_transferID;
                std::cout << "sending transferID: " << next_transferID << std::endl;
                outgoing_events.push({ {inpacket.senderID},enet_packet_create(data,sizeof(data),ENET_PACKET_FLAG_RELIABLE),0 });
            }
            {
                fh.target = connected_users.left().at(inpacket.senderID);
                mpv::serialization::Bytes data = mpv::serialization::Serialized<ClientOpcode>(INCOMING_FILE_HEADER) + (mpv::serialization::Serialized<size_t>(next_transferID++) + mpv::serialization::to_bytes(fh));
                outgoing_events.push({ {*targetID},enet_packet_create(data.get_array(),data.size(),ENET_PACKET_FLAG_RELIABLE),0 });
            }
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            enet_uint8 data[sizeof(ClientOpcode) + sizeof(size_t)] = { FILE_HEADER_ERROR };
            *reinterpret_cast<size_t*>(data + sizeof(ClientOpcode)) = client_temp_fileID;
            outgoing_events.push({ {inpacket.senderID},enet_packet_create(data,sizeof(data),ENET_PACKET_FLAG_RELIABLE),0 });
        }
    }
    void push_success(size_t id, ClientOpcode op, enet_uint8 channel) {
        outgoing_events.push({ {id},enet_packet_create(&op,sizeof(ClientOpcode),ENET_PACKET_FLAG_RELIABLE),channel });
    }
    void push_failure(size_t id, ClientOpcode op, enet_uint8 channel, const std::exception& e) {
        size_t size = strlen(e.what());
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[sizeof(ClientOpcode) + size]{ op });
        mpv::copy_n(data.get() + sizeof(ClientOpcode), e.what(), size);
        outgoing_events.push({ {id},enet_packet_create(data.get(),size,ENET_PACKET_FLAG_RELIABLE),channel });
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
            push_success(inpacket.senderID, SIGNIN_SUCCESS, 0);
            std::cout << "User created <" << username << ">\n";
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.senderID, SIGNIN_ERROR, 0, ex);
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
            
            if(connected_users.left().contains(inpacket.senderID)) throw peer_already_has_user_associated();
            if (!is_valid_username(username)) throw invalid_username();
            if (!users.get_dict().contains(username)) throw not_existing_user(username);
            if (connected_users.right().contains(username)) throw already_connected_user(username);
            if (crypto_pwhash_str_verify(users.get_dict().at(username).c_str(), password.c_str(), password.size()) != 0) throw wrong_password();
            
            connected_users.insert(inpacket.senderID,username);
            push_success(inpacket.senderID, LOGIN_SUCCESS, 0);
            std::cout << "User logged <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.senderID, LOGIN_ERROR, 0, ex);
        }
    }
    void log_out(const InPacket& inpacket) {
        try {
            if (inpacket.packet->dataLength > sizeof(ServerOpcode)) throw broken_packet(); // Delete user request only contains the opcode.
            auto it = connected_users.left().find(inpacket.senderID);
            if (it == connected_users.left().end()) throw not_logged_in();
            mpv::String username = it->val;
            connected_users.erase_by_left(inpacket.senderID);
            push_success(inpacket.senderID, LOGOUT_SUCCESS, 0);
            std::cout << "User logged out <" << username << ">\n";
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.senderID, LOGOUT_ERROR, 0, ex);
        }
    }
    void delete_user(const InPacket& inpacket) {
        try {
            if (inpacket.packet->dataLength > sizeof(ServerOpcode)) throw broken_packet(); // Delete user request only contains the opcode.
            auto it = connected_users.left().find(inpacket.senderID);
            if (it == connected_users.left().end()) throw not_logged_in();
            mpv::String username = it->val;
            connected_users.erase_by_left(inpacket.senderID);
            users.pop(username);
            push_success(inpacket.senderID, DELETE_USER_SUCCESS, 0);
            std::cout << "User deleted <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.senderID, DELETE_USER_ERROR, 0, ex);
        }
    }
    void send_message(const InPacket& inpacket) {
        try {
            const enet_uint8* end = inpacket.packet->data + inpacket.packet->dataLength;
            if (!connected_users.left().contains(inpacket.senderID)) throw not_logged_in();
            const enet_uint8* ptr = inpacket.packet->data + sizeof(ServerOpcode);
            if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
            mpv::String destinatarie((char*)ptr + 1, *ptr);
            ptr += sizeof(enet_uint8) + destinatarie.size();
            mpv::String message((char*)ptr, inpacket.packet->dataLength - destinatarie.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
            if (destinatarie.size() == 0) send_general_message(inpacket.senderID, message);
            else send_direct_message(inpacket.senderID, destinatarie, message);
            push_success(inpacket.senderID, SUBMIT_SUCCESS, 0);
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.senderID, SUBMIT_ERROR, 0, ex);
        }
    }
    void send_general_message(size_t senderID, const mpv::String& message) {
        const mpv::String& sender_user = connected_users.left().at(senderID);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + message.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ GENERAL_MESSAGE,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), message.c_str(), message.size());
        std::cout << "message(global)<" << sender_user << ">: " << message << std::endl;
        outgoing_events.push({ mpv::Vector<size_t>(connected_users.left().keys().begin(), connected_users.left().keys().end()),enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE) });
    }
    void send_direct_message(size_t senderID, const mpv::String& destinatarie, const mpv::String& message) {
        mpv::Optional<size_t> target = connected_users.right().get(destinatarie);
        if (!target.has_value()) throw target_user_not_found();
        const mpv::String& sender_user = connected_users.left().at(senderID);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + message.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ DIRECT_MESSAGE,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), message.c_str(), message.size());
        std::cout << "message from <" << sender_user << "> to <" << destinatarie << ">: " << message << std::endl;
        outgoing_events.push({ {*target},enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE) });
    }
    void send_command(const InPacket& inpacket) {
        try {
            const enet_uint8* end = inpacket.packet->data + inpacket.packet->dataLength;
            if (!connected_users.left().contains(inpacket.senderID)) throw not_logged_in();
            const enet_uint8* ptr = inpacket.packet->data + sizeof(ServerOpcode);
            if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
            mpv::String destinatarie((char*)ptr + 1, *ptr);
            ptr += sizeof(enet_uint8) + destinatarie.size();
            mpv::String command((char*)ptr, inpacket.packet->dataLength - destinatarie.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
            if (destinatarie.size() == 0) send_general_command(inpacket.senderID, command);
            else send_direct_command(inpacket.senderID, destinatarie, command);
            push_success(inpacket.senderID, SUBMIT_SUCCESS, 0);
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            push_failure(inpacket.senderID, SUBMIT_ERROR, 0, ex);
        }
    }
    void send_general_command(size_t senderID, const mpv::String& command) {
        const mpv::String& sender_user = connected_users.left().at(senderID);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
        mpv::Vector<size_t> targets(connected_users.left().keys().begin(), connected_users.left().keys().end());
        targets.remove(senderID);
        outgoing_events.push({ mpv::move(targets),enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE)});
        std::cout << "command(global)<" << sender_user << ">: " << command << std::endl;
    }
    void send_direct_command(size_t senderID, const mpv::String& destinatarie, const mpv::String& command) {
        mpv::Optional<size_t> target = connected_users.right().get(destinatarie);
        if (!target.has_value()) throw target_user_not_found();
        const mpv::String& sender_user = connected_users.left().at(senderID);
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
        outgoing_events.push({ {*target},enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE) });
        std::cout << "command from <" << sender_user << "> to <" << destinatarie << ">: " << command << std::endl;
    }
    /*
    send message()
        procesar mensaje(destinatario,mensaje)
        si destinatario vacio
            enviar a todos mensaje(usuario que lo envia,mensaje)
        sino
            enviar a destinatario mensaje(usuario que lo envia,mensaje)

    */

public:
	Aplication() : server(SERVER_PORT, 40, 2) {
		if (server.get_host() == nullptr) {
			std::cerr << "An error occurred while creating an ENet server host\n";
			exit(EXIT_FAILURE);
		}
	}
	void start() {
        running = true;
        network_thread = std::thread(&Aplication::network_loop, this);
        logic_thread = std::thread(&Aplication::logic_loop, this);
        file_relay_thread = std::thread(&Aplication::transfer_router_loop, this);
	}
    void stop() {
        running = false;
        incoming_events.close();
        outgoing_events.close();
        if (network_thread.joinable()) network_thread.join();
        if (logic_thread.joinable()) logic_thread.join();
        if (file_relay_thread.joinable()) file_relay_thread.join();
    }
};