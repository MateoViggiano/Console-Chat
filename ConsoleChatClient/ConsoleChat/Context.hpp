#pragma once
#include <enet/enet.h>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include "ImGuiConsole.hpp"
#include <viggiano.hpp>
#include <AssetManager.hpp>
#include <ENetClient.hpp>
#include "State.hpp"
#include <common.hpp>
class Context {
private:
	mpv::Queue<std::string> message_queue;
public:
	Client client = Client(1, 0, 0);
	sf::RenderWindow window = sf::RenderWindow(sf::VideoMode({ 1920, 1080 }), "Chat", sf::State::Windowed);
	mpv::StateMachine<State> state_manager;
	AssetManager asset_manager;
	std::string username;
	bool logged_in = false;
	mpv::Optional<std::string> poll_message() {
		if (message_queue.empty()) return {};
		else return message_queue.pop();
	}
	Context() = default;
	Context(Client&& client, sf::RenderWindow&& window = sf::RenderWindow(sf::VideoMode({ 1920, 1080 }), "Chat", sf::State::Windowed)) :client(mpv::move(client)), window(mpv::move(window)) {}
	void push_message(const std::string& msg) {
		message_queue.push(msg);
	}
	void add_user_request(const std::string& password) {
		constexpr size_t offset = sizeof(ServerOpcode) + 2 * sizeof(enet_uint8);
		const size_t data_size = offset + username.size() + password.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ ADD_USER,(enet_uint8)username.size(),(enet_uint8)password.size() });
		mpv::copy_n(data.get() + offset, username.c_str(), username.size());
		mpv::copy_n(data.get() + offset + username.size(), password.c_str(), password.size());
		client.send_packet(data.get(), data_size);
	}
	void log_in_request(const std::string& password) {
		constexpr size_t offset = sizeof(ServerOpcode) + 2 * sizeof(enet_uint8);
		const size_t data_size = offset + username.size() + password.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ LOG_IN, (enet_uint8)username.size(), (enet_uint8)password.size() });
		mpv::copy_n(data.get() + offset, username.c_str(), username.size());
		mpv::copy_n(data.get() + offset + username.size(), password.c_str(), password.size());
		client.send_packet(data.get(), data_size);
	}
	void log_out_request() {
		constexpr ServerOpcode op = LOG_OUT;
		client.send_packet(&op, sizeof(ServerOpcode));
	}
	void delete_user_request() {
		constexpr ServerOpcode op = DELETE_USER;
		client.send_packet(&op, sizeof(ServerOpcode));
	}
	//void send_general_message_request(const std::string& message) {
	//	const size_t data_size = sizeof(ServerOpcode) + message.size();
	//	mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ SEND_GENERAL_MESSAGE });
	//	mpv::copy_n(data.get() + sizeof(ServerOpcode), message.c_str(), message.size());
	//	client.send_packet(data.get(), data_size);
	//}
	void send_message_request(const std::string& destinatary, const std::string& message) {
		const size_t data_size = sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatary.size() + message.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ SEND_MESSAGE, (enet_uint8)destinatary.size() });
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8), destinatary.c_str(), destinatary.size());
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatary.size(), message.c_str(), message.size());
		client.send_packet(data.get(), data_size);
	}
	void send_command_request(const std::string& destinatary, const std::string& command) {
		const size_t data_size = sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatary.size() + command.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size]{ SEND_COMMAND, (enet_uint8)destinatary.size() });
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8), destinatary.c_str(), destinatary.size());
		mpv::copy_n(data.get() + sizeof(ServerOpcode) + sizeof(enet_uint8) + destinatary.size(), command.c_str(), command.size());
		client.send_packet(data.get(), data_size);
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
			window.create(sf::VideoMode(sf::Vector2u(1920, 1080)), "Pong", sf::State::Windowed);
		}
		else {
			window.close();
			window.create(sf::VideoMode(sf::Vector2u(1920, 1080)), "Pong", sf::State::Fullscreen);
		}
		is_fullscreen = !is_fullscreen;
	}
};