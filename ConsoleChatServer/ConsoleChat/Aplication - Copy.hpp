#include "ENetServer.hpp"
#include <iostream>
#include <sodium.h>
#include "viggiano.hpp"
#include "common.hpp"
#include "exceptions.hpp"
#include "DB.hpp"
struct Client {
    ENetPeer* peer;
    mpv::String user = "";
    bool is_logged()const noexcept {
        return user.empty();
    }
};
class Aplication {
	Server server;
    mpv::Database users = mpv::Database(L"database.txt");
    mpv::Map<size_t, Client> clients;
    size_t next_clientID=0;
    //mpv::Map<size_t, mpv::String> connected_users;
    //mpv::Map<size_t,ENetPeer*> clients
    using MapIterator = mpv::Map<size_t, Client>::iterator;
    using WrappedStr = mpv::serialization::Wrapper<mpv::String, unsigned char>;
    void send_success(ENetPeer* peer, ClientOpcode op) {
        server.send_packet_to_peer(peer, &op, sizeof(ClientOpcode));
    }
    void send_failure(ENetPeer* peer,ClientOpcode op, const std::exception& e) {
        size_t size = strlen(e.what());
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[sizeof(ClientOpcode) + size]{op});
        mpv::copy_n(data.get() + sizeof(ClientOpcode), e.what(), size);
        server.send_packet_to_peer(peer, data.get(), sizeof(ClientOpcode) + size);
    }
    void add_user() {
        try {
            constexpr size_t expected_header_size = sizeof(ServerOpcode) + 2;
            if (server.get_event().packet->dataLength < expected_header_size) throw broken_packet();
            enet_uint8* data = server.get_event().packet->data + sizeof(ServerOpcode);
            enet_uint8 username_size = *(data++);
            enet_uint8 password_size = *(data++);
            if (server.get_event().packet->dataLength < expected_header_size + username_size + password_size) throw broken_packet();
            mpv::String username((char*)data, username_size);
            data += username_size;
            mpv::String password((char*)data, password_size);
            mpv::String hashed_password((size_t)crypto_pwhash_STRBYTES - 1);
            if (!is_valid_username(username)) throw invalid_username();
            if (users.get_dict().contains(username)) throw existing_user(username);
            if (int error = crypto_pwhash_str(hashed_password.c_str(), password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) throw hasher_exception(error);
            users.insert(username,hashed_password);
            send_success(server.get_event().peer, SIGNIN_SUCCESS);
            std::cout << "User created <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            send_failure(server.get_event().peer,SIGNIN_ERROR,ex);
        }
    }
    void log_in() {
        try {
            constexpr size_t expected_header_size = sizeof(ServerOpcode) + 2;
            if (server.get_event().packet->dataLength < expected_header_size) throw broken_packet();
            enet_uint8* data = server.get_event().packet->data + sizeof(ServerOpcode);
            enet_uint8 username_size = *(data++);
            enet_uint8 password_size = *(data++);
            if (server.get_event().packet->dataLength < expected_header_size + username_size + password_size) throw broken_packet();
            mpv::String username((char*)data, username_size);
            data += username_size;
            mpv::String password((char*)data, password_size);
            ENetPeer* peer = server.get_event().peer;
            if (connected_users.values().any([peer](ENetPeer* p) {return p == peer;})) throw peer_already_has_user_associated();
            if (!is_valid_username(username)) throw invalid_username();
            if (!users.get_dict().contains(username)) throw not_existing_user(username);
            if (connected_users.contains(username)) throw already_connected_user(username);
            if (crypto_pwhash_str_verify(users.get_dict().at(username).c_str(), password.c_str(), password.size()) != 0) throw wrong_password();
            
            *reinterpret_cast<MapIterator*>(&peer->data) = connected_users.insert(mpv::MapPair<mpv::String, ENetPeer*>(username, peer));
            // reinterpreto el void* como un MapIterator, el cual internamente solo es un Tree::NodePtr
            // El usuario conectado matchea con su peer en el diccionario, y a la vez su peer almacena en peer->data un puntero al nodo de connected_users en el que se encuentra el usuario.
            send_success(server.get_event().peer, LOGIN_SUCCESS);
            std::cout << "User logged <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            send_failure(server.get_event().peer, LOGIN_ERROR, ex);
        }

    }
    void log_out() {
        try {
            if (server.get_event().packet->dataLength > sizeof(ServerOpcode)) throw broken_packet(); // Delete user request only contains the opcode.
            ENetPeer* peer = server.get_event().peer;
            if (*reinterpret_cast<MapIterator*>(&peer->data) == connected_users.end()) throw not_logged_in();
            mpv::String username = connected_users.pop(*reinterpret_cast<MapIterator*>(&peer->data))->key;
            *reinterpret_cast<MapIterator*>(&peer->data) = connected_users.end();
            send_success(server.get_event().peer, LOGOUT_SUCCESS);
            std::cout << "User logged out <" << username << ">\n";
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            send_failure(server.get_event().peer, LOGOUT_ERROR, ex);
        }
    }
    void delete_user() {
        try {
            if (server.get_event().packet->dataLength > sizeof(ServerOpcode)) throw broken_packet(); // Delete user request only contains the opcode.
            ENetPeer* peer = server.get_event().peer;
            if (*reinterpret_cast<MapIterator*>(&peer->data) == connected_users.end()) throw not_logged_in();
            users.pop((*reinterpret_cast<MapIterator*>(&peer->data))->key);
            mpv::String username = connected_users.pop(*reinterpret_cast<MapIterator*>(&peer->data))->key;
            *reinterpret_cast<MapIterator*>(&peer->data) = connected_users.end();
            send_success(server.get_event().peer, DELETE_USER_SUCCESS);
            std::cout << "User deleted <" << username << ">\n";
        }catch (const std::exception& ex) {
            puts(ex.what());
            send_failure(server.get_event().peer, DELETE_USER_ERROR, ex);
        }
    }
    void send_general_command(const mpv::String& command) {
        ENetPeer* sender_peer = server.get_event().peer;
        const mpv::String& sender_user = (*reinterpret_cast<MapIterator*>(&sender_peer->data))->key;
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
        ENetPacket* packet = enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE);
        for (ENetPeer* peer : connected_users.values()) {
            if (peer != sender_peer) enet_peer_send(peer, 0, packet);
        }
        std::cout << "command(global)<" << sender_user << ">: " << command << std::endl;
    }
    void send_direct_command(const mpv::String& destinatary, const mpv::String& command) {
        mpv::Optional<ENetPeer*> peer = connected_users.get(destinatary);
        if (!peer.has_value()) throw target_user_not_found();
        ENetPeer* sender_peer = server.get_event().peer;
        const mpv::String& sender_user = (*reinterpret_cast<MapIterator*>(&sender_peer->data))->key;
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + command.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ COMMAND,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), command.c_str(), command.size());
        ENetPacket* packet = enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(*peer, 0, packet);
        std::cout << "command from <" << sender_user << "> to <" << destinatary << ">: " << command << std::endl;
    }
    void send_command() {
        try {
            const enet_uint8* end = server.get_event().packet->data + server.get_event().packet->dataLength;
            if (*reinterpret_cast<MapIterator*>(&server.get_event().peer->data) == connected_users.end()) throw not_logged_in();
            const enet_uint8* ptr = server.get_event().packet->data + sizeof(ServerOpcode);
            if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
            mpv::String destinatary((char*)ptr + 1, *ptr);
            ptr += sizeof(enet_uint8) + destinatary.size();
            mpv::String command((char*)ptr, server.get_event().packet->dataLength - destinatary.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
            if (destinatary.size() == 0) send_general_command(command);
            else send_direct_command(destinatary, command);
            send_success(server.get_event().peer, SUBMIT_SUCCESS);
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            send_failure(server.get_event().peer, SUBMIT_ERROR, ex);
        }
    }
    void send_general_message(const mpv::String& message) {
        ENetPeer* sender_peer = server.get_event().peer;
        const mpv::String& sender_user = (*reinterpret_cast<MapIterator*>(&sender_peer->data))->key;
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + message.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size] {GENERAL_MESSAGE,(enet_uint8)sender_user.size()});
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), message.c_str(), message.size());
        ENetPacket* packet = enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE);
        for (ENetPeer* peer : connected_users.values()) {
            enet_peer_send(peer, 0, packet);
        }
        std::cout << "message(global)<" << sender_user << ">: " << message << std::endl;
    }
    void send_direct_message(const mpv::String& destinatary,const mpv::String& message) {
        mpv::Optional<ENetPeer*> peer = connected_users.get(destinatary);
        if (!peer.has_value()) throw target_user_not_found();
        ENetPeer* sender_peer = server.get_event().peer;
        const mpv::String& sender_user = (*reinterpret_cast<MapIterator*>(&sender_peer->data))->key;
        size_t data_size = sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size() + message.size();
        mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ DIRECT_MESSAGE,(enet_uint8)sender_user.size() });
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8), sender_user.c_str(), sender_user.size());
        mpv::copy_n(data.get() + sizeof(ClientOpcode) + sizeof(enet_uint8) + sender_user.size(), message.c_str(), message.size());
        ENetPacket* packet = enet_packet_create(data.get(), data_size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(*peer, 0, packet);
        std::cout << "message from <" << sender_user << "> to <" << destinatary << ">: " << message << std::endl;
    }
    /*
    send message()
        procesar mensaje(destinatario,mensaje)
        si destinatario vacio
            enviar a todos mensaje(usuario que lo envia,mensaje)
        sino
            enviar a destinatario mensaje(usuario que lo envia,mensaje)

    */
    void send_message() {
        try {
            const enet_uint8* end = server.get_event().packet->data + server.get_event().packet->dataLength;
            if (*reinterpret_cast<MapIterator*>(&server.get_event().peer->data) == connected_users.end()) throw not_logged_in();
            const enet_uint8* ptr = server.get_event().packet->data + sizeof(ServerOpcode);
            if (ptr + sizeof(enet_uint8) + *ptr > end) throw broken_packet();
            mpv::String destinatary((char*)ptr + 1,*ptr);
            ptr += sizeof(enet_uint8) + destinatary.size();
            mpv::String message((char*)ptr, server.get_event().packet->dataLength - destinatary.size() - sizeof(enet_uint8) - sizeof(ServerOpcode));
            if (destinatary.size() == 0) send_general_message(message);
            else send_direct_message(destinatary, message);
            send_success(server.get_event().peer, SUBMIT_SUCCESS);
        }
        catch (const std::exception& ex) {
            puts(ex.what());
            send_failure(server.get_event().peer, SUBMIT_ERROR, ex);
        }
    }
    void process_packet() {
        ENetPacket* packet = server.get_event().packet;
        try {
            if (packet->dataLength < sizeof(ServerOpcode)) throw broken_packet();
            switch (*reinterpret_cast<ServerOpcode*>(packet->data)) {
            case ADD_USER: add_user();break;
            case DELETE_USER: delete_user();break;
            case LOG_IN: log_in();break;
            case LOG_OUT: log_out();break;
            case SEND_MESSAGE: send_message();break;
            case SEND_COMMAND: send_command();break;
            default: throw broken_packet();
            }
        }catch (const std::exception& ex) {
            puts(ex.what());
        }
        enet_packet_destroy(packet);
    }
public:
	Aplication() : server(SERVER_PORT, 40) {
		if (server.get_host() == nullptr) {
			std::cerr << "An error occurred while creating an ENet server host\n";
			exit(EXIT_FAILURE);
		}
	}
	void start() {
        while (true) {
            while (server.host_service(1000) > 0) {
                switch (server.get_event().type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "A new client connected from " << server.get_event().peer->address.host << ":" << server.get_event().peer->address.port << ".\n";
                    *reinterpret_cast<MapIterator*>(&server.get_event().peer->data) = connected_users.end(); // Inicializa peer->data como un iterador nulo
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    puts("Packet recieved");
                    process_packet();
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << server.get_event().peer->address.host << ":" << server.get_event().peer->address.port << " disconnected.\n";
                    connected_users.del(*reinterpret_cast<MapIterator*>(&server.get_event().peer->data)); // Borra el usuario de usuarios_conectados si es que esta ahi
                    break;
                }
            }
        }
	}
};