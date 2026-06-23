#pragma once
#include <iostream>
#include <thread>
#include "State.hpp"
#include <common.hpp>
#include "Context.hpp"
#include "Menu.hpp"
#include "UserMenu.hpp"
class Program {
	Context context;
public:
	Program(const mpv::String& ip) : context(Client(1,0,0),sf::RenderWindow(sf::VideoMode({ 1200, 800 }), "Chat", sf::State::Windowed)) {
		if (!ImGui::SFML::Init(context.window)) exit(EXIT_FAILURE);
		if (context.client.get_host() == nullptr) {
			std::cerr << "An error occurred while creating an Enet client host.\n";
			exit(EXIT_FAILURE);
		}
		context.client.set_server_address(ip.c_str(), SERVER_PORT);
		context.client.connect();
		if (context.client.get_server_peer() == nullptr) {
			std::cerr << "No available peers for initialiting ENet connection.\n";
			exit(EXIT_FAILURE);
		}
		if (context.client.host_service(5000) > 0 && context.client.get_event().type == ENET_EVENT_TYPE_CONNECT) {
			std::cout << "Connection to " << context.client.get_server_address().port << " succeeded.\n";
		}
		else {
			context.client.reset_peer();
			std::cout << "Connection to " << context.client.get_server_address().port << " failed.\n";
			exit(EXIT_FAILURE);
		}
		context.asset_manager.add_font(TITLE, "C:/Windows/Fonts/vinet.ttf");
		context.asset_manager.add_font(TEXTBOX, "C:/Windows/Fonts/YuGothR.ttc");
		context.asset_manager.add_font(BUTTON, "C:/Windows/Fonts/YuGothB.ttc");
		context.state_manager.push(new Menu(context));
	}
	~Program() {
		ImGui::SFML::Shutdown();
	}
	void start() {
		while (context.window.isOpen()) {
			context.window.clear();
			process_server_event();
			context.state_manager.process_change();
			context.state_manager.current().process_imput();
			context.state_manager.current().update();
			context.state_manager.current().draw();

		}
	}
private:
	static ClientOpcode get_opcode(const void* data) {
		return *reinterpret_cast<const ClientOpcode*>(data);
	}
	std::string process_general_message() {
		enet_uint8* data = context.client.get_event().packet->data + sizeof(ClientOpcode);
		std::string sender((char*)data + sizeof(enet_uint8), *data);
		data += sizeof(enet_uint8) + sender.size();
		std::string message((char*)data, context.client.get_event().packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		std::string str = "(Global)" + sender + ": " + message;
		return str;
	}
	std::string process_direct_message() {
		enet_uint8* data = context.client.get_event().packet->data + sizeof(ClientOpcode);
		std::string sender((char*)data + sizeof(enet_uint8), *data);
		data += sizeof(enet_uint8) + sender.size();
		std::string message((char*)data, context.client.get_event().packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		std::string str = sender + ": " + message;
		return str;
	}
	std::pair<std::string, std::string> precess_command() {
		enet_uint8* data = context.client.get_event().packet->data + sizeof(ClientOpcode);
		std::string sender((char*)data + sizeof(enet_uint8), *data);
		data += sizeof(enet_uint8) + sender.size();
		std::string command((char*)data, context.client.get_event().packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		return { sender,command };
	}
	std::string process_failure() {
		enet_uint8* data = context.client.get_event().packet->data + sizeof(ClientOpcode);
		std::string errormessage((char*)data, context.client.get_event().packet->dataLength - sizeof(ClientOpcode));
		return errormessage;
	}
	void process_packet() {
		switch (ClientOpcode op = get_opcode(context.client.get_event().packet->data)) {
		case SIGNIN_ERROR:
		case LOGIN_ERROR:
		case DELETE_USER_ERROR:
		case LOGOUT_ERROR:
		case SUBMIT_ERROR:
			std::cout << op << ": " << process_failure() << std::endl;
			break;
		case LOGOUT_SUCCESS:
		case DELETE_USER_SUCCESS:
			std::cout << op << std::endl;
			context.logged_in = false;
			context.state_manager.pop_until_find<Menu>();
			break;
		case SIGNIN_SUCCESS:
			std::cout << op << std::endl;
			context.state_manager.pop_until_find<Menu>();
			break;
		case LOGIN_SUCCESS:
			std::cout << op << std::endl;
			context.logged_in = true;
			context.state_manager.pop_until_find<Menu>();
			context.state_manager.push(new UserMenu(context));
			break;
		case SUBMIT_SUCCESS:
			std::cout << op << std::endl;
			break;
		case DIRECT_MESSAGE:
			context.push_message(process_direct_message());
			break;
		case GENERAL_MESSAGE:
			context.push_message(process_general_message());
			break;
		case COMMAND: {
			auto input = precess_command();
			std::cout << input.first << " sent a command: " << input.second << std::endl;
			std::thread new_thread(run_command, input.second);
			new_thread.detach();
			break;
		}
		default:
			puts("Unknown Opcode. Packet may be corrupt\a");
		}
	}
	void process_server_event() {
		while (context.client.host_service() > 0) {
			switch (context.client.get_event().type) {
			case ENET_EVENT_TYPE_RECEIVE:
				puts("Packet recieved");
				process_packet();
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				context.window.close();
				puts("Disconnection succeeded");
				break;
			}
		}
	}

};