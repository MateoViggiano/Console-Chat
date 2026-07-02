#include "Aplication.hpp"
#include <iostream>

int main() {
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);
    Aplication app;
    app.start();
    mpv::String s;
    while (s != "exit") {
        std::cin >> s;
    }
    app.stop();
}