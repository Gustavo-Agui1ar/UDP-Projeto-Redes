
#include <iostream>
#include "Server/ChromaServer.hpp"
#include "Client/ChromaClient.hpp"

int main() {


    ChromaClient client(5, 1024);
    client.connectToServer("127.0.0.1", 8080);
    
    std::string filename;
    char choice = 's';

    while (choice == 's' && client.isConnected()) {
        std::cout << "Digite o nome do arquivo a ser solicitado: ";
        std::cin >> filename;

        client.sendData(filename.c_str(), filename.size());

        std::cout << "Deseja solicitar outro arquivo? (s/n): ";
        std::cin >> choice;
    }

    return 0;
}