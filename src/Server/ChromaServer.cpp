
#include "ChromaServer.hpp"
#include <iostream>
#include <fstream>
#include <thread>

ChromaServer::ChromaServer(int winSize, int bufSize, sockaddr_in clientAddr, int sockfd): ChromaProtocol(winSize, bufSize) 
{
    this->sockfd = sockfd; // Usar o socket já criado por ChromaProtocol?
    this->clientAddr = clientAddr;
    timers = vector<Timer>(bufSize);
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    ifstream file(filename, ios::binary);
    
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo: " << filename << endl;
        return;
    }

    vector<char> buffer(chunkSize);

    while (base <= getNextSeqNum() && !file.eof())
    {
        while ((getNextSeqNum() < base + windowSize) && (file.read(buffer.data(), chunkSize) || file.gcount() > 0)) {

            Packet pkt(getNextSeqNum(), buffer, ChromaMethod::POST);
            sendBuffer[pkt.seqNum] = pkt;

            cout << "Servidor enviou pacote " << pkt.seqNum << " (" << file.gcount() << " bytes)" << endl;
            
            timers[getNextSeqNum()].start(1000, [this, pkt]() {
                cout << "Timeout seq " << pkt.seqNum << " -> retransmitindo\n";
                sendPacket(pkt, clientAddr);
            });

            sendPacket(pkt, clientAddr);
            nextSeqNum++;
        }

        receiveData();
    }

    file.close();
    cout << "Envio concluído: " << filename << endl;
}

// Receber ACKs/NACKs do cliente
void ChromaServer::receiveData() {

    Packet pkt;

    if (recvPacket(pkt) > 0) {
        if (pkt.method == ChromaMethod::ACK && !isCorrupted(pkt)) {
            cout << "ACK recebido para seq: " << pkt.seqNum << endl;
            if(pkt.seqNum == base) 
                base++;
            timers[pkt.seqNum].stop();      
        }
        else if (pkt.method == ChromaMethod::NACK) {
            cout << "NACK recebido para seq: " << pkt.seqNum << endl;
            sendPacket(sendBuffer[pkt.seqNum], clientAddr);

            timers[pkt.seqNum].start(1000, [this, pkt]() {
                cout << "Timeout seq " << pkt.seqNum << " -> retransmitindo\n";
                sendPacket(sendBuffer[pkt.seqNum], clientAddr);
            });
        }
    }
}


