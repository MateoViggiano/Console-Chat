#pragma once
#include <viggiano>
#include <enet/enet.h>
constexpr enet_uint16 SERVER_PORT = 6767;
enum ServerOpcode : enet_uint8 { ADD_USER, DELETE_USER, LOG_IN, LOG_OUT, SEND_MESSAGE, SEND_COMMAND, SEND_FILE };//opcodes que interpreta el servidor
enum ClientOpcode : enet_uint8 { SIGNIN_SUCCESS, SIGNIN_ERROR ,LOGIN_SUCCESS, LOGIN_ERROR, LOGOUT_SUCCESS, LOGOUT_ERROR, DELETE_USER_SUCCESS, DELETE_USER_ERROR, SUBMIT_SUCCESS, SUBMIT_ERROR, DIRECT_MESSAGE, GENERAL_MESSAGE, COMMAND };//opcodes que interpreta el cliente
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
	default: return out << "!!! UNKNOWN OPCODE !!!\a";
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
static constexpr size_t getID(const ENetPeer* peer) {
	return (*reinterpret_cast<const mpv::Map<size_t, ENetPeer*>::iterator*>(&peer->data))->key;
}