#include <iostream>
#include "Server/ChromaServiceHost.hpp"

int main() {
    int windowSize = WINDOW_SIZE;
    int port = 8080;

    ChromaServiceHost serverManager(windowSize, port);
    serverManager.start();

    return 0;
}