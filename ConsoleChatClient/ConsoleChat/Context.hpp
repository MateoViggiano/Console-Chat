#pragma once
#include <iostream>
#include <enet/enet.h>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include "ImGuiConsole.hpp"
#include <ThreadSafeQueue.hpp>
#include <AssetManager.hpp>
#include <ENetClient.hpp>
#include "State.hpp"
#include <common.hpp>
struct OutPacket {
	ENetPacket* packet;
	enet_uint8 channel;
};
struct oFile {
	mpv::String filename;
	std::ofstream stream;
	oFile(const oFile&) = delete;
	oFile& operator=(const oFile&) = delete;
	oFile() = default;
	oFile(const mpv::String& filename) : filename(filename), stream(filename.c_str(), std::ios::out | std::ios::binary) {}
	oFile(oFile&& other) : filename(std::move(filename)), stream(std::move(other.stream)) {}
	oFile& operator=(oFile&& other) {
		filename = std::move(other.filename);
		stream = std::move(other.stream);
		return *this;
	}
};
struct iFile {
	mpv::String filename;
	std::ifstream stream;
	iFile(const iFile&) = delete;
	iFile& operator=(const iFile&) = delete;
	iFile() = default;
	iFile(const mpv::String& filename) : filename(filename), stream(filename.c_str(), std::ios::in | std::ios::binary) {}
	iFile(iFile&& other) : filename(std::move(filename)), stream(std::move(other.stream)) {}
	iFile& operator=(iFile&& other) {
		filename = std::move(other.filename);
		stream = std::move(other.stream);
		return *this;
	}
};
static std::ostream& operator<<(std::ostream& out, const iFile& f) {
	return out << f.filename;
}
static std::ostream& operator<<(std::ostream& out, const oFile& f) {
	return out << f.filename;
}
class Context {
private:
	mpv::Queue<std::string> message_queue;
public:
	mpv::tsQueue<ENetPacket*> incoming_packets, incoming_file_chunks;
	mpv::tsQueue<OutPacket> outgoing_packets;
	Client client = Client(2, 0, 0);
	sf::RenderWindow window;
	mpv::StateMachine<State> state_manager;
	AssetManager asset_manager;
	std::string username;
	size_t next_temp_fileID = 0;
	mpv::Map<size_t, mpv::String> outgoing_file_headers;
	mpv::Map<size_t, oFile> incoming_files;
	mpv::Map<size_t, iFile> outgoing_files;
	mpv::tsQueue<mpv::Pair<size_t, mpv::String>> incoming_files_queue, outgoing_files_queue;
	bool logged_in = false;
	std::atomic<bool> running = false;
	mpv::Optional<std::string> poll_message() {
		if (message_queue.empty()) return {};
		else return message_queue.pop();
	}
	Context(const Context&) = delete;
	Context& operator=(const Context&) = delete;
	Context() = default;
	Context(Client&& client) :client(mpv::move(client)) {}
	void push_message(const std::string& msg) {
		message_queue.push(msg);
	}
	void send_file_header(const std::string& destinatarie, const std::string& filepath) {
		std::filesystem::path path(filepath);
		if (!std::filesystem::is_regular_file(path)) {
			std::cout << path << " not found\n";
			return;
		}
		path = path.filename();
		FileHeader fh{mpv::String(path.native().begin(), path.native().end()),destinatarie.c_str()};
		mpv::serialization::Bytes data = mpv::serialization::Serialized<ServerOpcode>(FILE_HEADER) + (mpv::serialization::Serialized<size_t>(next_temp_fileID) + mpv::serialization::to_bytes(fh));
		outgoing_packets.push({ enet_packet_create(data.get_array(),data.size(),ENET_PACKET_FLAG_RELIABLE),0});
		outgoing_file_headers[next_temp_fileID++] = filepath.c_str();
	}
	void add_user(const std::string& password) {
		constexpr size_t offset = sizeof(ServerOpcode) + 2 * sizeof(enet_uint8);
		const size_t data_size = offset + username.size() + password.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ ADD_USER,(enet_uint8)username.size(),(enet_uint8)password.size() });
		mpv::copy_n(data.get() + offset, username.c_str(), username.size());
		mpv::copy_n(data.get() + offset + username.size(), password.c_str(), password.size());
		outgoing_packets.push({ enet_packet_create(data.get(),data_size,ENET_PACKET_FLAG_RELIABLE),0 });
	}
	void log_in(const std::string& password) {
		constexpr size_t offset = sizeof(ServerOpcode) + 2 * sizeof(enet_uint8);
		const size_t data_size = offset + username.size() + password.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ LOG_IN, (enet_uint8)username.size(), (enet_uint8)password.size() });
		mpv::copy_n(data.get() + offset, username.c_str(), username.size());
		mpv::copy_n(data.get() + offset + username.size(), password.c_str(), password.size());
		outgoing_packets.push({ enet_packet_create(data.get(),data_size,ENET_PACKET_FLAG_RELIABLE),0 });
	}
	void log_out() {
		constexpr ServerOpcode op = LOG_OUT;
		outgoing_packets.push({ enet_packet_create(&op,sizeof(op),ENET_PACKET_FLAG_RELIABLE),0});
	}
	void delete_user() {
		constexpr ServerOpcode op = DELETE_USER;
		outgoing_packets.push({ enet_packet_create(&op,sizeof(op),ENET_PACKET_FLAG_RELIABLE),0 });
	}
	void send_message(const std::string& destinatarie, const std::string& message) {
		const size_t data_size = sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatarie.size() + message.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ SEND_MESSAGE, (enet_uint8)destinatarie.size() });
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8), destinatarie.c_str(), destinatarie.size());
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatarie.size(), message.c_str(), message.size());
		outgoing_packets.push({ enet_packet_create(data.get(),data_size,ENET_PACKET_FLAG_RELIABLE),0 });
	}
	void send_command(const std::string& destinatarie, const std::string& command) {
		const size_t data_size = sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatarie.size() + command.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ SEND_COMMAND, (enet_uint8)destinatarie.size() });
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8), destinatarie.c_str(), destinatarie.size());
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatarie.size(), command.c_str(), command.size());
		outgoing_packets.push({ enet_packet_create(data.get(),data_size,ENET_PACKET_FLAG_RELIABLE),0 });
	}
	void process_window_event(sf::Event event) {
		if (event.is<sf::Event::Closed>())
			window.close();
		if (event.is<sf::Event::KeyPressed>()) {
			if (event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::F11) {
				toggle_fullscreen();
			}
		}
	}
	void toggle_fullscreen() {
		static bool is_fullscreen = false;
		if (is_fullscreen) {
			window.close();
			window.create(sf::VideoMode(sf::Vector2u(1920, 1080)), "Chat", sf::State::Windowed);
		}
		else {
			window.close();
			window.create(sf::VideoMode(sf::Vector2u(1920, 1080)), "Chat", sf::State::Fullscreen);
		}
		is_fullscreen = !is_fullscreen;
	}
};