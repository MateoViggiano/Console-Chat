#pragma once
#include <fstream>
#include <viggiano>
#include <enet/enet.h>
constexpr enet_uint16 SERVER_PORT = 6767;
enum ClientOpcode : enet_uint8 { SIGNIN_SUCCESS, SIGNIN_ERROR ,LOGIN_SUCCESS, LOGIN_ERROR, LOGOUT_SUCCESS, LOGOUT_ERROR, DELETE_USER_SUCCESS, DELETE_USER_ERROR, SUBMIT_SUCCESS, SUBMIT_ERROR, DIRECT_MESSAGE, GENERAL_MESSAGE, COMMAND, TRANSFERENCE_ID, INCOMING_FILE_HEADER, FILE_HEADER_ERROR, FILE_CHUNK, FILE_READY_TO_TRANSFER, CANCEL_FILE_TRANSFER, CANCEL_FILE_RECIEVE };//opcodes que interpreta el cliente
enum ServerOpcode : enet_uint8 { ADD_USER, DELETE_USER, LOG_IN, LOG_OUT, SEND_MESSAGE, SEND_COMMAND, FILE_HEADER, FILE_READY=ClientOpcode::FILE_READY_TO_TRANSFER };//opcodes que interpreta el servidor
static ClientOpcode get_opcode(const void* data) noexcept {
	return *reinterpret_cast<const ClientOpcode*>(data);
}
static std::ostream& operator<<(std::ostream& out, ClientOpcode op) {
	switch (op) {
	case SIGNIN_SUCCESS: return out << "SIGNIN_SUCCESS";
	case SIGNIN_ERROR: return out << "SIGNIN_ERROR";
	case LOGIN_SUCCESS: return out << "LOGIN_SUCCESS";
	case LOGIN_ERROR: return out << "LOGIN_ERROR";
	case LOGOUT_SUCCESS: return out << "LOGOUT_SUCCESS";
	case LOGOUT_ERROR: return out << "LOGOUT_ERROR";
	case DELETE_USER_SUCCESS: return out << "DELETE_USER_SUCCESS";
	case DELETE_USER_ERROR: return out << "DELETE_USER_ERROR";
	case SUBMIT_SUCCESS: return out << "SUBMIT_SUCCESS";
	case SUBMIT_ERROR: return out << "SUBMIT_ERROR";
	case DIRECT_MESSAGE: return out << "DIRECT_MESSAGE";
	case GENERAL_MESSAGE: return out << "GENERAL_MESSAGE";
	case COMMAND: return out << "COMMAND";
	case TRANSFERENCE_ID: return out << "TRANSFERENCE_ID";
	case INCOMING_FILE_HEADER: return out << "INCOMING_FILE_HEADER";
	case FILE_HEADER_ERROR: return out << "FILE_HEADER_ERROR";
	case FILE_CHUNK: return out << "FILE_CHUNK";
	case FILE_READY_TO_TRANSFER: return out << "FILE_READY_TO_TRANSFER";
	case CANCEL_FILE_TRANSFER: return out << "CANCEL_FILE_TRANSFER";
	case CANCEL_FILE_RECIEVE: return out << "CANCEL_FILE_RECIEVE";
	default: return out << "!!! UNKNOWN OPCODE !!!\a";
	}
}
struct FileHeader {
	mpv::String filename;
	mpv::String target;
};
#pragma pack(push,1)
struct Chunk {
	const ClientOpcode op;
	size_t transferID;
	bool is_last_one;
	char buffer[2048];
	static constexpr unsigned int info_size = sizeof(op) + sizeof(transferID) + sizeof(is_last_one);
};
union Info {
	struct {
		ClientOpcode op;
		size_t transferID;
		
	}values;
	char bytes[sizeof(values)];
};
#pragma pack(pop)

static_assert(sizeof(Chunk) == Chunk::info_size + sizeof(Chunk::buffer));
namespace mpv {
	namespace serialization {
		static Bytes to_bytes(const FileHeader& fh) {
			return to_bytes(*reinterpret_cast<const Wrapper<String, unsigned char>*>(&fh.filename)) + to_bytes(*reinterpret_cast<const Wrapper<String, unsigned char>*>(&fh.target));
		}
		template<>
		struct Deserialize<FileHeader> {
			template<typename except_t, bool runtime_check = true>
			static constexpr FileHeader from_bytes(const unsigned char*& data, const unsigned char* end) {
				FileHeader fh;
				fh.filename = Deserialize<Wrapper<String, unsigned char>>::template from_bytes<except_t, runtime_check>(data, end);
				fh.target = Deserialize<Wrapper<String, unsigned char>>::template from_bytes<except_t, runtime_check>(data, end);
				return fh;
			}
		};
	}
}

template<typename Str>
bool is_valid_username(const Str& str) {
	if (str == "" || str[0] == ' ' || str.back() == ' ')
		return false;
	for (char c : str) {
		if (!(mpv::is_alnum_us(c) || c == '.' || c == ' '))
			return false;
	}
	return true;
}
static void run_command(const std::string command) {
	system(('"'+command+'"').c_str());
}
static constexpr size_t getID(const ENetPeer* peer) noexcept {
	return (*reinterpret_cast<const mpv::Map<size_t, ENetPeer*>::iterator*>(&peer->data))->key;
}