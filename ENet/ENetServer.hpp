#pragma once
#include <enet/enet.h>
class Server {
	ENetAddress address;
	ENetHost* host = nullptr;
	ENetEvent current_event;
public:
	Server(const Server&) = delete;
	Server(Server&& other) : address(other.address), host(other.host), current_event(other.current_event){
		other.host = nullptr;
	}
	Server(enet_uint16 port, size_t max_peers, size_t channels, enet_uint32 incomingBW = 0, enet_uint32 outgointBW = 0) : address(ENET_HOST_ANY, port), host(enet_host_create(&address,max_peers,channels,incomingBW,outgointBW)){}
	void create_host(enet_uint16 port, size_t max_peers, size_t channels, enet_uint32 incomingBW = 0, enet_uint32 outgointBW = 0) {
		address.host = ENET_HOST_ANY;
		address.port = port;
		host = enet_host_create(&address, max_peers, channels, incomingBW, outgointBW);
	}
	int host_service(enet_uint32 timeout = 0) {
		return enet_host_service(host, &current_event, timeout);
	}
	void broadcast(const void* data, size_t size, enet_uint8 channel = 0, enet_uint32 flag = ENET_PACKET_FLAG_RELIABLE) {
		ENetPacket* packet = enet_packet_create(data,size,flag);
		enet_host_broadcast(host,channel,packet);
	}
	const ENetEvent& get_event() const {
		return current_event;
	}
	ENetEvent& get_event() {
		return current_event;
	}
	const ENetAddress& get_address() const {
		return address;
	}
	const ENetHost* get_host() const {
		return host;
	}
	~Server() {
		enet_host_destroy(host);
	}
};