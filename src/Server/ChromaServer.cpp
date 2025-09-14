
#include "ChromaServer.hpp"
#include <iostream>
#include <fstream>
#include <thread>

ChromaServer::ChromaServer(int winSize, int bufSize, sockaddr_in clientAddr, int sockfd): ChromaProtocol(winSize, bufSize) 
{
    this->sockfd = sockfd; // Usar o socket já criado por ChromaProtocol?
    this->clientAddr = clientAddr;
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo: " << filename << endl;
        return;
    }

    int seqNum = 0;
    vector<char> buffer(chunkSize);

    while ((nextSeqNum < base + windowSize) && (file.read(buffer.data(), chunkSize) || file.gcount() > 0)) {

        Packet pkt(seqNum++, buffer, ChromaMethod::POST);
        sendBuffer[pkt.seqNum] = pkt;

        //sendPacket(pkt);
        cout << "Servidor enviou pacote " << pkt.seqNum << " (" << file.gcount() << " bytes)" << endl;

        //adicionar janela deslizante e logica que impete o envio de mais pacotes que o tamanho da janela em sendPacket(pkt);
    }

    file.close();
    cout << "Envio concluído: " << filename << endl;
}

// Receber ACKs/NACKs do cliente
void ChromaServer::receiveData() {

    Packet pkt;

    //Criar Timer para os packets

    if (recvPacket(pkt) > 0) {
        if (pkt.method == ChromaMethod::ACK && !isCorrupted(pkt)) {
            cout << "ACK recebido para seq: " << pkt.seqNum << endl;
            if(pkt.seqNum == base) 
                base++;      
        }
        else if (pkt.method == ChromaMethod::NACK) {
            cout << "NACK recebido para seq: " << pkt.seqNum << endl;
            sendPacket(sendBuffer[pkt.seqNum], clientAddr);
        }
    }
}
