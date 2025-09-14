
#include <iostream>
#include "Server/ChromaServer.hpp"
#include "Client/ChromaClient.hpp"

int main() {
    /*
    int windowSize = 5;
    int bufferSize = 1024;
    int port = 8080; 
    
    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(port);
    
    ChromaServer server(windowSize, bufferSize, clientAddr, -1);
    
    server.sendData("teste.txt");
    */

    ChromaClient client(5, 1024);
    client.connectToServer("127.0.0.1", 8080);
    client.waitClientRequest();

    return 0;
}