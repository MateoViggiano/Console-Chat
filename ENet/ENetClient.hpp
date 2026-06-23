#pragma once
#include <enet/enet.h>
class Client {
	ENetHost* host = nullptr;
	ENetAddress server_address = {};
	ENetPeer* server_peer = nullptr;
	ENetEvent current_event = {};
public:
	Client(const Client&) = delete;
	Client(Client&& other) :host(other.host), server_address(other.server_address), server_peer(other.server_peer), current_event(other.current_event) {
		other.host = nullptr;
		other.server_peer = nullptr;
	}
	Client(size_t channels = 1, enet_uint32 incomingBW = 0, enet_uint32 outgointBW = 0) 
		: host(enet_host_create(nullptr, 1, channels, incomingBW, outgointBW)){}
	void create_host(size_t channels = 1, enet_uint32 incomingBW = 0, enet_uint32 outgointBW = 0) {
		host = enet_host_create(nullptr, 1, channels, incomingBW, outgointBW);
	}
	void set_server_address(const char* host_name, enet_uint16 port) {
		enet_address_set_host(&server_address, host_name);
		server_address.port = port;
	}
	void connect(size_t channels = 1, enet_uint32 data = 0) {
		server_peer = enet_host_connect(host, &server_address, channels, data);
	}
	void disconnect(enet_uint32 data = 0) {
		enet_peer_disconnect(server_peer, data);
	}
	void reset_peer() {
		enet_peer_reset(server_peer);
	}
	int host_service(enet_uint32 timeout = 0) {
		return enet_host_service(host, &current_event, timeout);
	}
	void send_packet(const void* data,size_t size,enet_uint8 channel = 0, enet_uint32 flag = ENET_PACKET_FLAG_RELIABLE) {
		ENetPacket* packet = enet_packet_create(data, size, flag);
		enet_peer_send(server_peer, channel, packet);
	}
	const ENetEvent& get_event() const{
		return current_event;
	}
	ENetEvent& get_event() {
		return current_event;
	}
	const ENetAddress& get_server_address() const{
		return server_address;
	}
	const ENetHost* get_host() const{
		return host;
	}
	const ENetPeer* get_server_peer() const{
		return server_peer;
	}
	~Client() {
		enet_host_destroy(host);
	}
};