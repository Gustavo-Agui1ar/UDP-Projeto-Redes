#include "ChromaClient.hpp"
#include <arpa/inet.h>

ChromaClient::ChromaClient(int winSize, int bufSize): ChromaProtocol(winSize, bufSize) {}

ChromaClient::~ChromaClient() {
    disconnect();
}

void ChromaClient::connectToServer(const char* ip, int port) {
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        throw std::runtime_error("Endereço inválido ou não suportado");
    }

    connected = true;
    std::cout <<    "Cliente conectado ao servidor " << ip << ": " << port << std::endl;
}

void ChromaClient::disconnect() {
    if (connected) {
        close(sockfd);
        connected = false;
        serverAddr = {};
        serverResponseAddr = {};
        std::cout << "Cliente desconectado." << std::endl;
    }
}

void ChromaClient::sendData(const char* data, size_t len) {
    std::cout << "Enviando dados ao Servidor..." << std::endl;

    Packet pkt(0, std::vector<char>(data, data + len), ChromaMethod::GET, addr);

    if (sendPacket(pkt, serverAddr) < 0) {
        throw std::runtime_error("Falha ao enviar dados para o servidor");
    }

    int ret = waitResponse(5);

    if (ret > 0) {
        
        if (recvPacket(pkt) < 0) {
            cout << "Erro ao receber resposta do servidor" << endl;
        }

        if (pkt.method == ChromaMethod::NACK) {
            std::cerr << "Servidor não conseguiu localizar o arquivo solicitado." << std::endl;
            return;
        }

        if(pkt.method == ChromaMethod::ACK) {
            std::cout << "Servidor pronto para enviar o arquivo." << std::endl;
        }

        serverResponseAddr = pkt.srcAddr;

        std::cout << "Requisição enviada ao servidor host: "
                  << inet_ntoa(serverAddr.sin_addr) << ":"
                  << ntohs(serverAddr.sin_port) << std::endl;

    } else if (ret == 0) {
        cout << "Servidor não respondeu dentro do tempo limite" << endl;
    } else {
        cout << "Erro no select()" << endl;
    }
    receiveData();
}

void ChromaClient::receiveData() {
    // implementar lógica de recebimento
    std::cout << "Recebendo pacote: " << std::endl;
}

void ChromaClient::waitClientRequest() {
    std::string filename;
    char choice = 's';

    while (choice == 's') {
        std::cout << "Digite o nome do arquivo a ser solicitado: ";
        std::cin >> filename;

        sendData(filename.c_str(), filename.size());

        std::cout << "Deseja solicitar outro arquivo? (s/n): ";
        std::cin >> choice;
    }
}


