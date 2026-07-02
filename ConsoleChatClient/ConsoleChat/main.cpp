#include <iostream>
#include "Program.hpp"
int main() {
	if (enet_initialize() != 0) {
		std::cerr << "An error occurred while initializing ENet.\n";
		return EXIT_FAILURE;
	}
	atexit(enet_deinitialize);
	mpv::String serverip;
	std::cout << "Enter the server's ip: ";
	std::cin >> serverip;
	Program prog(serverip);
	prog.start();
	while (prog.is_running()) {
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}