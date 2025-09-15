#include <iostream>
#include "Server/ChromaServerManager.hpp"

int main() {
    int windowSize = 5;
    int bufferSize = 1024;
    int port = 8080;

    ChromaServerManager serverManager(windowSize, bufferSize, port);
    serverManager.start();

    return 0;
}