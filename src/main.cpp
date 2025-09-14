
#include <iostream>
#include "Server/ChromaServer.hpp"

int main() {

    int windowSize = 5; // Exemplo de tamanho da janela
    int bufferSize = 1024; // Exemplo de tamanho do buffer
    int port = 8080; // Porta do servidor

    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(port);

    ChromaServer server(windowSize, bufferSize, clientAddr, -1);

    server.sendData("teste.txt");

    return 0;
}