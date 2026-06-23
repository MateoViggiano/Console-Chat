#pragma once
#include <exception>
#include <viggiano>

class broken_packet : public std::exception {
public:
	const char* what() const noexcept override {
		return "Broken packet recieved";
	}
};
class existing_user : public std::exception {
public:
	const mpv::String user;
	existing_user(const mpv::String& user):user(user) {}
	const char* what() const noexcept override {
		return "User already exists";
	}
};
class not_existing_user : public std::exception {
public:
	const mpv::String user;
	not_existing_user(const mpv::String& user) :user(user) {}
	const char* what() const noexcept override {
		return "User doesn't exist";
	}
};
class already_connected_user : public std::exception {
public:
	const mpv::String user;
	already_connected_user(const mpv::String& user) :user(user) {}
	const char* what() const noexcept override {
		return "User already connected";
	}
};
class wrong_password : public std::exception {
	const char* what() const noexcept override {
		return "Password doesn't match";
	}
};
class invalid_username : public std::exception {
public:
	const char* what() const noexcept override {
		return "Invalid username";
	}
};
class hasher_exception : public std::exception {
public:
	const int error_code;
	hasher_exception(int error_code) : error_code(error_code){}
	const char* what() const noexcept override {
		return "Hasher error";
	}
};
class not_logged_in : public std::exception {
public:
	const char* what() const noexcept override {
		return "Not logged in";
	}
};
class peer_already_has_user_associated : public std::exception {
public:
	const char* what() const noexcept override {
		return "Peer already has user associated";
	}
};
class target_user_not_found : public std::exception {
public:
	const char* what() const noexcept override {
		return "Target user not found";
	}
};