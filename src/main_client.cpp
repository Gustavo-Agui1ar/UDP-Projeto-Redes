
#include <iostream>
#include "Server/ChromaServer.hpp"
#include "Client/ChromaClient.hpp"

int main() {


    ChromaClient client(5, 1024);
    client.connectToServer("127.0.0.1", 8080);
    client.waitClientRequest();

    return 0;
}