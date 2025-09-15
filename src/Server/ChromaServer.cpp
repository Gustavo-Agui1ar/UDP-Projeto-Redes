
#include "ChromaServer.hpp"
#include <fstream>

ChromaServer::ChromaServer(int winSize, int bufSize, sockaddr_in clientAddr): ChromaProtocol(winSize, bufSize) 
{
    this->clientAddr = clientAddr;
    timers = std::vector<Timer>(bufSize);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("Erro ao bindar socket do servidor");
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr*)&addr, &len) < 0) {
        throw std::runtime_error("Erro ao obter porta atribuída ao servidor");
    }

    std::cout << "Servidor rodando na porta: " << ntohs(addr.sin_port) << std::endl << "Com ip: " << inet_ntoa(addr.sin_addr) << std::endl;
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    
    ifstream file(filename, ios::binary);
    
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo: " << filename << endl;
        vector<char> emptyData;
        Packet pkt(0, emptyData, ChromaMethod::NACK, addr);
        sendPacket(pkt, clientAddr);
        return;
    }

    Packet pkt = {0, vector<char>(), ChromaMethod::ACK, addr}; // Comfirmação de que o arquivo foi encontrado e será enviado

    vector<char> buffer(chunkSize);

    while (base <= getNextSeqNum() && !file.eof())
    {
        while ((getNextSeqNum() < base + windowSize) && (file.read(buffer.data(), chunkSize) || file.gcount() > 0)) {

            Packet pkt(getNextSeqNum(), buffer, ChromaMethod::POST, addr);
            sendBuffer[pkt.seqNum] = pkt;

            cout << "Servidor enviou pacote " << pkt.seqNum << " (" << file.gcount() << " bytes)" << endl;
            
            setTimerAndSendPacket(pkt, 1000, clientAddr);
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
            
            timers[pkt.seqNum].stop();
            sendBuffer[pkt.seqNum].received = true;     

            if(pkt.seqNum == base) {
                while(base < bufferSize && sendBuffer[base].received) {
                    base++;
                }
            }
        }
        else if (pkt.method == ChromaMethod::NACK) {
            cout << "NACK recebido para seq: " << pkt.seqNum << endl;
            setTimerAndSendPacket(sendBuffer[pkt.seqNum], 1000, pkt.srcAddr);
        }
    }
}

void ChromaServer::setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest) {
    
    timers[pkt.seqNum].start(timeoutMs, [this, pkt, dest]() {
        cout << "Timeout seq " << pkt.seqNum << " -> retransmitindo\n";
        sendPacket(pkt, dest);
    });
    sendPacket(pkt, dest);
}

