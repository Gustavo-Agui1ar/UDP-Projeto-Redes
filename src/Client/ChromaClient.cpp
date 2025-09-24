#include "ChromaClient.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

// Cores para debug
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

ChromaClient::ChromaClient(int winSize, int bufSize)
    : ChromaProtocol(winSize, bufSize) {}

ChromaClient::~ChromaClient() {
    disconnect();
}

void ChromaClient::connectToServer(const char* ip, int port) {
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        throw runtime_error(RED "Endereço inválido ou não suportado" RESET);
    }

    connected = true;
    if (!quietMode) {
        cout << GREEN << "Cliente conectado ao servidor " 
             << ip << ":" << port << RESET << endl;
    }
}

void ChromaClient::disconnect() {
    if (!connected) return;

    close(sockfd);
    connected = false;
    serverAddr = {};
    serverResponseAddr = {};

    if (!quietMode) {
        cout << YELLOW << "Cliente desconectado." << RESET << endl;
    }
}

void ChromaClient::sendData(const char* data, size_t len) {
    string fileRequested(data, len);
    size_t pos = fileRequested.find_last_of('.');
    extensionFile = (pos != string::npos) ? fileRequested.substr(pos + 1) : "bin";

    if (!quietMode) {
        cout << CYAN << "Solicitando arquivo: " << fileRequested << RESET << endl;
    }

    Packet request(0, vector<char>(data, data + len), ChromaFlag::GET, addr);
    if (sendPacket(request, serverAddr) < 0) {
        throw runtime_error(RED "Falha ao enviar requisição para o servidor" RESET);
    }

    if (!waitResponse(5)) {
        if (!quietMode) cerr << RED << "Timeout aguardando resposta inicial." << RESET << endl;
        return;
    }

    Packet pkt;
    if (recvPacket(pkt) <= 0 || pkt.flag != ChromaFlag::ACK || isCorrupted(pkt)) {
        if (!quietMode) cerr << RED << "Falha no handshake inicial." << RESET << endl;
        return;
    }

    if (!quietMode) {
        cout << GREEN << "Contato estabelecido com a thread do servidor." << RESET << endl;
    }

    serverResponseAddr = pkt.srcAddr;
    readFileMetadata(pkt);

    // Confirma recebimento dos metadados
    Packet ackMeta(0, {}, ChromaFlag::ACK, addr);
    sendPacket(ackMeta, serverResponseAddr);

    // Prepara buffer para recebimento dos dados
    sendBuffer.clear();
    sendBuffer.resize(bufferSize);
    base = 0;
    nextSeqNum = 0;

    receiveData();
}

void ChromaClient::setQuietMode(bool quiet) {
    quietMode = quiet;
}

void ChromaClient::receiveData() {
    if (!quietMode) {
        cout << CYAN << "Aguardando pacotes do servidor..." << RESET << endl;
    }

    bool transmissionEnded = false;

    ofstream file(("arquivo_reconstruido_" + filename + extensionFile).c_str(),ios::out | ios::binary);
    if (!file.is_open()) {
        throw runtime_error(RED "Erro ao criar arquivo de saída" RESET);
    }

    while (!transmissionEnded) {
        Packet pkt;

        if (!waitResponse(10) || recvPacket(pkt) <= 0) {
            if (!quietMode) cerr << RED << "Timeout ou erro de recepção." << RESET << endl;
            break;
        }

        if (isCorrupted(pkt)) {
            if (!quietMode) cerr << YELLOW << "Pacote corrompido descartado." << RESET << endl;
            continue;
        }

        switch (pkt.flag) {
            case ChromaFlag::DATA: {
                int idx = pkt.seqNum % bufferSize;

                if (sendBuffer[idx].received) {
                    if (!quietMode) {
                        cout << MAGENTA << "Pacote duplicado Seq=" << pkt.seqNum 
                             << " → reenviando ACK." << RESET << endl;
                    }
                    sendConfirmation(pkt.seqNum, ChromaFlag::ACK, serverResponseAddr);
                    break;
                }

                sendBuffer[idx] = pkt;
                sendBuffer[idx].received = true;

                if (!quietMode) {
                    cout << BLUE << "Pacote Seq=" << pkt.seqNum 
                         << " (" << pkt.data.size() << " bytes) recebido." << RESET << endl;
                }

                // Escreve em ordem os pacotes confirmados
                while (sendBuffer[base % bufferSize].received && base < totalPackets) {
                    Packet& inOrder = sendBuffer[base % bufferSize];
                    file.write(inOrder.data.data(), inOrder.data.size());
                    inOrder.received = false;
                    base++;
                }

                sendConfirmation(pkt.seqNum, ChromaFlag::ACK, serverResponseAddr);
                break;
            }

            case ChromaFlag::ACK:
                if (!quietMode) cout << GREEN << "Fim de transmissão." << RESET << endl;
                transmissionEnded = true;
                break;

            case ChromaFlag::NACK:
                if (!quietMode) cerr << RED << "Servidor não encontrou o arquivo." << RESET << endl;
                transmissionEnded = true;
                break;

            default:
                if (!quietMode) cerr << YELLOW << "Flag desconhecida ignorada." << RESET << endl;
                break;
        }
    }

    file.flush();
    file.close();
    if (!quietMode) {
        cout << GREEN << "Arquivo salvo com sucesso!" << RESET << endl;
    }
}

void ChromaClient::readFileMetadata(const Packet& pkt) {
    string dataStr(pkt.data.begin(), pkt.data.end());
    istringstream iss(dataStr);
    string fname, ext, sizeStr, totalStr;

    if (!getline(iss, fname, '\0') ||
        !getline(iss, ext, '\0') ||
        !getline(iss, sizeStr, '\0') ||
        !getline(iss, totalStr, '\0')) {
        throw runtime_error(RED "Erro ao interpretar metadados." RESET);
    }

    filename = fname.substr(0, fname.find_last_of('.'));
    extensionFile = ext;
    fileSize = stoll(sizeStr);
    totalPackets = stoi(totalStr);

    if (!quietMode) {
        cout << GREEN << "Metadados recebidos:" << RESET << endl;
        cout << "Arquivo: " << filename << endl;
        cout << "Extensão: " << extensionFile << endl;
        cout << "Tamanho: " << fileSize << " bytes" << endl;
        cout << "Pacotes esperados: " << totalPackets << endl;
    }
}
