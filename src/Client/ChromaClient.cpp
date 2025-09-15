#include "ChromaClient.hpp"
#include <arpa/inet.h>
#include <iostream> 
#include <fstream>  

using namespace std;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"


ChromaClient::ChromaClient(int winSize, int bufSize): ChromaProtocol(winSize, bufSize) {}

ChromaClient::~ChromaClient() {
    disconnect();
}

void ChromaClient::connectToServer(const char* ip, int port) {
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        throw std::runtime_error(RED "Endereço inválido ou não suportado" RESET);
    }

    connected = true;
    cout << GREEN << "Cliente conectado ao servidor " << ip << ": " << port << RESET << endl;
}

void ChromaClient::disconnect() {
    if (connected) {
        close(sockfd);
        connected = false;
        serverAddr = {};
        serverResponseAddr = {};
        cout << YELLOW << "Cliente desconectado." << RESET << endl;
    }
}

void ChromaClient::sendData(const char* data, size_t len) {
    std::string fileRequested(data, len);

    size_t pos = fileRequested.find_last_of('.');
    if (pos != std::string::npos) {
        extensionFile = fileRequested.substr(pos + 1); 
    } else {
        extensionFile = "bin"; 
    }

    cout << CYAN << "Enviando requisição ao Servidor..." << RESET << endl;
    Packet pkt(0, std::vector<char>(data, data + len), ChromaMethod::GET, addr);

    if (sendPacket(pkt, serverAddr) < 0) {
        throw std::runtime_error(RED "Falha ao enviar requisição para o servidor" RESET);
    }

    receiveData();
}

void ChromaClient::receiveData() {
    cout << CYAN << "Aguardando resposta do servidor e iniciando recepção..." << RESET << endl;
    bool transmissionEnded = false;
    bool initialContactMade = false;

    while (!transmissionEnded) {
        Packet pkt;

        if (waitResponse(10) > 0) { 
            if (recvPacket(pkt) <= 0) {
                cout << RED << "Erro ao receber pacote ou conexão encerrada." << RESET << endl;
                return;
            }
        } else {
            cout << RED << "Timeout: Nenhum pacote recebido do servidor dentro do tempo limite." << RESET << endl;
            return;
        }

        if (!initialContactMade) {
            serverResponseAddr = pkt.srcAddr;
            initialContactMade = true;
            cout << GREEN << "Contato estabelecido com a thread do servidor." << RESET << endl;
        }

        if (isCorrupted(pkt)) {
            cout << YELLOW << "Pacote corrompido recebido, ignorando..." << RESET << endl;
            continue;
        }

        switch (pkt.method) {
            case ChromaMethod::POST:
                if (pkt.seqNum >= base && pkt.seqNum < base + windowSize)
                {
                    if (pkt.seqNum > base) 
                    {
                        cout << YELLOW << "Pacote recebido fora de ordem: " << pkt.seqNum << RESET << endl;
                    } else 
                    { 
                        cout << GREEN << "Pacote recebido na ordem correta: " << pkt.seqNum << RESET << endl;
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
                cout << GREEN << "Fim de transmissão recebido do servidor." << RESET << endl;
                transmissionEnded = true;
                break;

            case ChromaMethod::NACK:
                cerr << RED << "Servidor não conseguiu localizar o arquivo solicitado." << RESET << endl;
                transmissionEnded = true;
                sendBuffer.clear();
                break;

            default:
                cout << YELLOW << "Método desconhecido recebido, ignorando..." << RESET << endl;
                break;
        }
    }

    if (!sendBuffer.empty()) {
        cout << GREEN << "Recepção concluída." << RESET << endl;
        recreateFile(("arquivo_recebido." + extensionFile).c_str());
    } else {
        cout << YELLOW << "Recepção finalizada sem dados para gravar." << RESET << endl;
    }
}

void ChromaClient::recreateFile(const char* filename) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(RED "Erro ao criar arquivo: " + std::string(filename) + RESET);
    }

    cout << CYAN << "Recriando o arquivo como '" << filename << "'..." << RESET << endl;
    for (size_t i = 0; i < base; ++i) {
        if (sendBuffer[i].received) {
             file.write(sendBuffer[i].data.data(), sendBuffer[i].data.size());
        }
    }

    file.close();
    cout << GREEN << "Arquivo recriado com sucesso!" << RESET << endl;
}