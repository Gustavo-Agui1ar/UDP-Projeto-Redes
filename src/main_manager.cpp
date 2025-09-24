#include <iostream>
#include "Server/ChromaServiceHost.hpp"

int main() {
    int windowSize = 5;
    int bufferSize = 20;
    int port = 8080;

    ChromaServiceHost serverManager(windowSize, bufferSize, port);
    serverManager.start();

    return 0;
}