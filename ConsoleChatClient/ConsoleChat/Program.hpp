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
	std::thread program_thread, network_thread, file_thread;
public:
	Program(const mpv::String& ip) : context(Client(2,0,0)) {
		if (context.client.get_host() == nullptr) {
			std::cerr << "An error occurred while creating an Enet client host.\n";
			exit(EXIT_FAILURE);
		}
		context.client.set_server_address(ip.c_str(), SERVER_PORT);
		context.client.connect(2);
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
	}
	void start() {
		context.running = true;
		network_thread = std::thread(&Program::network_loop, this);
		program_thread = std::thread(&Program::program_loop, this);
		file_thread = std::thread(&Program::file_loop, this);
	}
	~Program() {
		context.running = false;
		context.incoming_packets.close();
		context.outgoing_packets.close();
		context.incoming_file_chunks.close();
		if (network_thread.joinable()) network_thread.join();
		if (program_thread.joinable()) program_thread.join();
		if (file_thread.joinable()) file_thread.join();
	}
	bool is_running()const{
		return context.running;
	}
private:
	void network_loop() {
		while (context.running) {
			while (context.client.host_service() > 0) {
				switch (context.client.get_event().type) {
				case ENET_EVENT_TYPE_RECEIVE:
					switch (context.client.get_event().channelID) {
					case 0:context.incoming_packets.push(context.client.get_event().packet);break;
					case 1:context.incoming_file_chunks.push(context.client.get_event().packet);break;
					}
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					context.running = false;
					puts("Disconnected");
					break;
				}
			}
			mpv::Optional<OutPacket> outpacket;
			while (context.outgoing_packets.try_pop(outpacket)) {
				context.client.send_packet(outpacket->packet, outpacket->channel);
			}
		}
	}
	void program_loop() {
		context.window.create(sf::VideoMode({ 1200, 800 }), "Chat", sf::State::Windowed);
		context.state_manager.push(new Menu(context));
		if (!ImGui::SFML::Init(context.window)) exit(EXIT_FAILURE);
		while (context.running && context.window.isOpen()) {
			context.window.clear();
			context.state_manager.process_change();
			process_packets();
			context.state_manager.current().process_imput();
			context.state_manager.current().update();
			context.state_manager.current().draw();
		}
		context.running = false;
		ImGui::SFML::Shutdown();
		context.window.close();
	}
	void file_loop() {
		while (context.running) {
			mpv::Optional<mpv::Pair<size_t, mpv::String>> opt;
			while (context.incoming_files_queue.try_pop(opt)) {
				oFile ofile(mpv::move(opt->x2));
				if (ofile.stream.is_open()) {
					context.incoming_files[opt->x1] = mpv::move(ofile);
					std::cout << "Succesfully opended incoming file: " << *context.incoming_files.find(opt->x1) << std::endl;
					Info data{ FILE_READY_TO_TRANSFER,opt->x1 };
					context.outgoing_packets.push({enet_packet_create(&data,sizeof(data),ENET_PACKET_FLAG_RELIABLE),0});
				}
				else std::cout << "Couldn't open " << ofile.filename << std::endl;
			}
			while (context.outgoing_files_queue.try_pop(opt)) {
				iFile ifile(mpv::move(opt->x2));
				if (ifile.stream.is_open()) {
					context.outgoing_files[opt->x1] = mpv::move(ifile);
					std::cout << "Succesfully opended outgoing file: " << *context.outgoing_files.find(opt->x1) << std::endl;
				}
				else std::cout << "Couldn't open " << ifile.filename << std::endl;
			}

			mpv::Optional<ENetPacket*> opt_packet;
			while (context.incoming_file_chunks.try_pop(opt_packet)) {
				switch (get_opcode(opt_packet.value()->data)) {
				case FILE_CHUNK: read_file_chunk(*opt_packet);break;
				case CANCEL_FILE_RECIEVE: cancel_file_recieve(*opt_packet);break;
				case CANCEL_FILE_TRANSFER: cancel_file_transfer(*opt_packet);break;
				}
			}
			mpv::Map<size_t, iFile>::iterator it = context.outgoing_files.begin();
			while (it != context.outgoing_files.end()) {
				Chunk chunk{ FILE_CHUNK,it->key };
				it->val.stream.read(chunk.buffer, sizeof(chunk.buffer));
				chunk.is_last_one = it->val.stream.eof();
				context.outgoing_packets.push({ enet_packet_create(&chunk,Chunk::info_size + it->val.stream.gcount(),ENET_PACKET_FLAG_RELIABLE),1 });
				if (chunk.is_last_one) context.outgoing_files.del(it++);
				else ++it;
			}
		}
	}
	void read_file_chunk(const ENetPacket* packet) {
		const Chunk& chunk = *reinterpret_cast<const Chunk*>(packet->data);
		size_t buffer_size = packet->dataLength - Chunk::info_size;
		auto it = context.incoming_files.find(chunk.transferID);
		if (context.incoming_files.end() == it) {
			std::cout << "FILE WITH ID " << chunk.transferID << " WASN'T FOUND\n";
			exit(10);
		}
		it->val.stream.write(chunk.buffer, buffer_size);
		if (chunk.is_last_one) std::cout << "File recieving complete: " << context.incoming_files.pop(it) << std::endl;
	}

	void cancel_file_recieve(const ENetPacket* packet) {
		const Info& data = *reinterpret_cast<const Info*>(packet->data);
		std::cout << "Cancel file recieve: " << context.incoming_files.pop_elem(data.values.transferID) << std::endl;
	}
	void cancel_file_transfer(const ENetPacket* packet) {
		const Info& data = *reinterpret_cast<const Info*>(packet->data);
		std::cout << "Cancel file transfer: " << context.outgoing_files.pop_elem(data.values.transferID) << std::endl;
	}
	std::string process_general_message(const ENetPacket* packet) {
		enet_uint8* data = packet->data + sizeof(ClientOpcode);
		std::string sender((char*)data + sizeof(enet_uint8), *data);
		data += sizeof(enet_uint8) + sender.size();
		std::string message((char*)data, packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		std::string str = "(Global)" + sender + ": " + message;
		return str;
	}
	std::string process_direct_message(const ENetPacket* packet) {
		enet_uint8* data = packet->data + sizeof(ClientOpcode);
		std::string sender((char*)data + sizeof(enet_uint8), *data);
		data += sizeof(enet_uint8) + sender.size();
		std::string message((char*)data, packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		std::string str = sender + ": " + message;
		return str;
	}
	std::pair<std::string, std::string> precess_command(const ENetPacket* packet) {
		enet_uint8* data = packet->data + sizeof(ClientOpcode);
		std::string sender((char*)data + sizeof(enet_uint8), *data);
		data += sizeof(enet_uint8) + sender.size();
		std::string command((char*)data, packet->dataLength - sizeof(ClientOpcode) - sizeof(enet_uint8) - sender.size());
		return { sender,command };
	}
	std::string process_failure(const ENetPacket* packet) {
		enet_uint8* data = packet->data + sizeof(ClientOpcode);
		std::string errormessage((char*)data, packet->dataLength - sizeof(ClientOpcode));
		return errormessage;
	}
	size_t process_transferenceID(const ENetPacket* packet) {
		size_t temp_fileID = *reinterpret_cast<size_t*>(packet->data + sizeof(ClientOpcode));
		size_t transferID = *reinterpret_cast<size_t*>(packet->data + sizeof(ClientOpcode) + sizeof(size_t));
		mpv::Optional<mpv::String> filename = context.outgoing_file_headers.pop_at(temp_fileID);
		if (!filename.has_value()) throw;
		context.not_ready_outgoing_file[transferID] = *filename;
		return transferID;
	}
	void set_file_ready(const ENetPacket* packet) {
		const Info& data = *reinterpret_cast<const Info*>(packet->data);
		mpv::Optional<mpv::MapPair<size_t,mpv::String>> x = context.not_ready_outgoing_file.pop_elem(data.values.transferID);
		std::cout << "Starting transference: " << x << std::endl;
		context.outgoing_files_queue.push({ x->key,x->val });
	}
	FileHeader process_incoming_file_header(const ENetPacket* packet) {
		mpv::serialization::Deserializer deserializer(packet->data + sizeof(ClientOpcode), packet->dataLength - sizeof(ClientOpcode));
		size_t transferID = deserializer.read<size_t>();
		FileHeader fh = deserializer.read<FileHeader>();
		context.incoming_files_queue.push({ transferID,fh.filename });
		return fh;
	}
	void process_packets() {
		mpv::Optional<ENetPacket*> opt_packet;
		while (context.incoming_packets.try_pop(opt_packet)) {
			mpv::uPtr<ENetPacket, decltype(&enet_packet_destroy)> packet(*opt_packet,enet_packet_destroy);
			switch (ClientOpcode op = get_opcode(packet->data)) {
			case SIGNIN_ERROR:
			case LOGIN_ERROR:
			case DELETE_USER_ERROR:
			case LOGOUT_ERROR:
			case SUBMIT_ERROR:
				std::cout << op << ": " << process_failure(packet.get()) << std::endl;
				break;
			case FILE_READY_TO_TRANSFER:
				std::cout << op << std::endl;
				set_file_ready(packet.get());
				break;
			case FILE_HEADER_ERROR:
				context.outgoing_file_headers.del_elem(*reinterpret_cast<size_t*>(packet->data+sizeof(ClientOpcode)));
				std::cout << op << std::endl;
				break;
			case TRANSFERENCE_ID:
				std::cout << op << ": " << process_transferenceID(packet.get()) << std::endl;
				break;
			case INCOMING_FILE_HEADER: {
				FileHeader fh = process_incoming_file_header(packet.get());
				std::cout << op << ": <" << fh.target << "> is sharing a file: " << fh.filename << std::endl;
			}	break;
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
				context.push_message(process_direct_message(packet.get()));
				break;
			case GENERAL_MESSAGE:
				context.push_message(process_general_message(packet.get()));
				break;
			case COMMAND: {
				auto input = precess_command(packet.get());
				std::cout << input.first << " sent a command: " << input.second << std::endl;
				std::thread new_thread(run_command, input.second);
				new_thread.detach();
				break;
			}
			default:
				puts("Unknown Opcode. Packet may be corrupt\a");
			}
		}
	}
};