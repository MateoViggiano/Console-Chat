#include <iostream>
#include <conio.h>
#include "Console.hpp"
#include <viggiano.hpp>
#include "ENetClient.hpp"
#include <common.hpp>
using std::cout;
using std::cin;
using std::endl;
ClientOpcode get_opcode(const void* data) {
	return *reinterpret_cast<const ClientOpcode*>(data);
}
class StateManager {
	Client client;
	mpv::String username, password;
	bool end_program = false;
public:
	enum class State :char {START_MENU,SIGN_IN,LOG_IN,USER_MENU,DELETE_USER,CHANGE_PASSWORD,CHAT} state = State::START_MENU;
	StateManager() : client(1,0,0) {
		if (client.get_host() == nullptr) {
			std::cerr << "An error occurred while creating an Enet client host.\n";
			exit(EXIT_FAILURE);
		}
		mpv::String ip;
		cout << "Enter the server ip: ";
		cin >> ip;
		client.set_server_address(ip.c_str(), SERVER_PORT);
		client.connect();
		if (client.get_server_peer() == nullptr) {
			std::cerr << "No available peers for initialiting ENet connection.\n";
			exit(EXIT_FAILURE);
		}
		if (client.host_service(5000) > 0 && client.get_event().type == ENET_EVENT_TYPE_CONNECT) {
			std::cout << "Connection to " << client.get_server_address().port << " succeeded.\n";
		}
		else {
			client.reset_peer();
			std::cout << "Connection to " << client.get_server_address().port << " failed.\n";
			exit(EXIT_FAILURE);
		}
	}
	void run() {
		while (!end_program) {
			switch (state) {
			case State::START_MENU: start_menu();break;
			case State::SIGN_IN: sign_in();break;
			case State::LOG_IN: log_in();break;
			case State::USER_MENU: user_menu();break;
			case State::DELETE_USER: delete_user();break;
			case State::CHANGE_PASSWORD: change_password();break;
			case State::CHAT: startchat();break;
			}
		}
	}
private:
	// States
	void start_menu() {
		bool repeat = true;
		cout << "1: log in\n"
			 << "2: sign in\n";
		while (repeat) {
			cout << ">> ";
			switch (static_cast<char>(_getch())) {
			case '1': state = State::LOG_IN;repeat = false;break;
			case '2': state = State::SIGN_IN;repeat = false;break;
			default: cout << "Invalid key" << endl;
			}
		}
	}
	mpv::String get_error_msg() {
		return mpv::String((char*)client.get_event().packet->data + sizeof(ClientOpcode), client.get_event().packet->dataLength - sizeof(ClientOpcode));
	}
	void await_login_response() {
		bool stop = false;
		while (client.host_service(10000) > 0 && !stop) {
			switch (client.get_event().type) {
			case ENET_EVENT_TYPE_RECEIVE:
				switch (get_opcode(client.get_event().packet->data)) {
				case LOGIN_SUCCESS:
					state = State::USER_MENU;
					break;
				case LOGIN_ERROR:
					cout << "Login error: " << get_error_msg() << endl;
					state = State::START_MENU;
					username = password = "";
					break;
				default:
					puts("Packet ignored");
				}
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				puts("Disconnected from server");
				break;
			}
		}
		if (state == State::LOG_IN) {
			state = State::START_MENU;
			puts("await_login_response timeout");
		}
	}
	void await_signin_response() {
		while (client.host_service(10000) > 0 && state == State::SIGN_IN) {
			switch (client.get_event().type) {
			case ENET_EVENT_TYPE_RECEIVE:
				switch (get_opcode(client.get_event().packet->data)) {
				case SIGNIN_SUCCESS:
					state = State::LOG_IN;
					log_in_request();
					await_login_response();
					break;
				case SIGNIN_ERROR:
					cout << "Signin error: " << get_error_msg() << endl;
					state = State::START_MENU;
					username = password = "";
					break;
				default:
					puts("Packet ignored");
				}
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				puts("Disconnected from server");
				exit(0);
				break;
			}
		}
		if (state == State::SIGN_IN) {
			state = State::START_MENU;
			puts("await_signin_response timeout");
		}
	}
	void sign_in() {
		bool repeat = true;
		cout << "Provide username" << endl;
		while (repeat) {
			cout << ">> ";
			cin >> username;
			if (is_valid_username(username)) {
				repeat = false;
			}
			else {
				cout << "invalid username" << endl;
				username.clear();
			}
		}
		cout << "Provide password" << endl;
		while (password == "") {
			cout << ">> ";
			cin >> password;
			if (password == "" || password.size() > 255) {
				cout << "Invalid password" << endl;
			}
		}
		add_user_request();
		await_signin_response();
	}
	void log_in() {
		bool repeat = true;
		cout << "Provide username" << endl;
		while (repeat) {
			cout << ">> ";
			cin >> username;
			if (is_valid_username(username)) {
				repeat = false;
			}
			else {
				cout << "invalid username" << endl;
				username.clear();
			}
		}
		cout << "Provide password" << endl;
		while (password == "") {
			cout << ">> ";
			cin >> password;
			if (password == "") {
				cout << "Invalid password" << endl;
			}
		}
		log_in_request();
		await_login_response();
	}
	void user_menu() {
		bool repeat = true;
		cout << "1: chat" << endl
			 << "2: change password" << endl
			 << "3: delete user" << endl;
		while (repeat) {
			cout << ">> ";
			switch (static_cast<char>(_getch())) {
			case '1': state = State::CHAT;repeat = false;break;
			case '2': state = State::CHANGE_PASSWORD;repeat = false;break;
			case '3': state = State::DELETE_USER;repeat = false;break;
			default: cout << "Invalid key" << endl;
			}
		}
	}
	void delete_user() {
		delete_user_request();

	}
	void change_password() {

	}

	void startchat() {
		sf::RenderWindow window(sf::VideoMode({ 1400, 800 }), "ConsoleChat", sf::State::Windowed);
		Console console(sf::Vector2f(window.getSize()));
		bool exit = false;
		while (!exit) {
			console.update_selected(window);
			while (client.host_service() > 0) {
				switch (client.get_event().type) {
				case ENET_EVENT_TYPE_RECEIVE:
					puts("Packet recieved");
					process_packet(console);
					console.printMessage((char*)client.get_event().packet->data);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					exit = true;
					puts("Disconnection succeeded");
					break;
				}
			}
			while (const std::optional<sf::Event> event = window.pollEvent()) {
				if (event->is<sf::Event::Closed>()) {
					window.close();
				}
				if (console.text_sent(*event)) {
					sf::String message = console.getAndClearInput();
					if (message == L"/exit") {
						client.disconnect();
					}
					else {
						//console.printMessage(message);
						send_general_message_request(message.toAnsiString());
						// Aquí procesa el mensaje como necesites
						//std::cout << "Mensaje recibido: " << message.toAnsiString() << std::endl;
					}
				}
				else console.update(*event);
			}

			window.clear();
			window.draw(console);
			window.display();
		}
		state = State::USER_MENU;
	}
	// Process from server
	mpv::String process_general_message() {
		enet_uint8* data = client.get_event().packet->data + sizeof(ClientOpcode);
		mpv::String sender((char*)data + sizeof(enet_uint8),*data);
		data += sizeof(enet_uint8) + sender.size();
		mpv::String message((char*)data, client.get_event().packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		mpv::String str = "(Global)" + sender + ": " + message;
		return str;
	}
	void process_packet(Console& console) {
		switch (get_opcode(client.get_event().packet->data)) {
		case GENERAL_MESSAGE:
			mpv::String msg = process_general_message();
			cout << msg;
			console.printMessage(msg.c_str());
			break;

		}
	}
	// Send to server
	void add_user_request() {
		constexpr size_t offset = sizeof(ServerOpcode) + 2 * sizeof(enet_uint8);
		const size_t data_size = offset + username.size() + password.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size] {ADD_USER,(enet_uint8)username.size(),(enet_uint8)password.size()});
		mpv::copy_n(data.get() + offset, username.c_str(), username.size());
		mpv::copy_n(data.get() + offset + username.size(), password.c_str(), password.size());
		client.send_packet(data.get(), data_size);
	}
	void log_in_request() {
		constexpr size_t offset = sizeof(ServerOpcode) + 2 * sizeof(enet_uint8);
		const size_t data_size = offset + username.size() + password.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size] {LOG_IN, (enet_uint8)username.size(), (enet_uint8)password.size()});
		mpv::copy_n(data.get() + offset, username.c_str(), username.size());
		mpv::copy_n(data.get() + offset + username.size(), password.c_str(), password.size());
		client.send_packet(data.get(), data_size);
	}
	void delete_user_request() {
		constexpr ServerOpcode op = DELETE_USER;
		client.send_packet(&op, sizeof(ServerOpcode));
	}
	void send_general_message_request(const std::string& message) {
		const size_t data_size = sizeof(ServerOpcode) + message.size();
		mpv::uPtr<enet_uint8[]> data(new enet_uint8[data_size] {SEND_GENERAL_MESSAGE});
		mpv::copy_n(data.get() + sizeof(ServerOpcode), message.c_str(), message.size());
		client.send_packet(data.get(), data_size);
	}
};