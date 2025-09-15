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

    std::cout << "Enviando requisição ao Servidor..." << std::endl;
    Packet pkt(0, std::vector<char>(data, data + len), ChromaMethod::GET, addr);

    if (sendPacket(pkt, serverAddr) < 0) {
        throw std::runtime_error("Falha ao enviar requisição para o servidor");
    }

    receiveData();
}

void ChromaClient::receiveData() {

    std::cout << "Aguardando resposta do servidor e iniciando recepção..." << std::endl;
    bool transmissionEnded = false;
    bool initialContactMade = false;

    while (!transmissionEnded) {
        Packet pkt;

        if (waitResponse(10) > 0) { 
            if (recvPacket(pkt) <= 0) {
                std::cout << "Erro ao receber pacote ou conexão encerrada." << std::endl;
                break;
            }
        } else {
            std::cout << "Timeout: Nenhum pacote recebido do servidor dentro do tempo limite." << std::endl;
            break;
        }

        if (!initialContactMade) {
            serverResponseAddr = pkt.srcAddr;
            initialContactMade = true;
            std::cout << "Contato estabelecido com a thread do servidor." << std::endl;
        }

        if (isCorrupted(pkt)) {
            cout << "Pacote corrompido recebido, ignorando..." << endl;
            continue;
        }

        switch (pkt.method) {
            case ChromaMethod::POST:
                if (pkt.seqNum >= base && pkt.seqNum < base + windowSize)
                {
                    if (pkt.seqNum > base) 
                    {
                        cout << "Pacote recebido fora de ordem: " << pkt.seqNum << endl;
                    } else 
                    { 
                        cout << "Pacote recebido na ordem correta: " << pkt.seqNum << endl;
                        base++;
                        while(base < bufferSize && sendBuffer[base].received)
                        {
                            base++;
                        }
                    }
                    sendBuffer[pkt.seqNum] = pkt;
                    sendBuffer[pkt.seqNum].received = true;
                }
                sendConfirmation(pkt.seqNum, ChromaMethod::ACK, serverResponseAddr);
                break;

            case ChromaMethod::ACK:
                cout << "Fim de transmissão recebido do servidor." << endl;
                transmissionEnded = true;
                break;

            case ChromaMethod::NACK:
                std::cerr << "Servidor não conseguiu localizar o arquivo solicitado." << std::endl;
                transmissionEnded = true;
                break;

            default:
                cout << "Método desconhecido recebido, ignorando..." << endl;
                break;
        }
    }

    std::cout << "Recepção concluída." << std::endl;
    recreateFile("arquivo_recebido.txt");
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

void ChromaClient::recreateFile(const char* filename) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Erro ao criar arquivo: " + std::string(filename));
    }

    for (const auto& pkt : sendBuffer) {
        file.write(pkt.data.data(), pkt.data.size());
    }

    file.close();
}


