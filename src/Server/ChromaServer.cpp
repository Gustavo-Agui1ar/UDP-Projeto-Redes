
#include "ChromaServer.hpp"
#include <fstream>
#include <fcntl.h> 

ChromaServer::ChromaServer(int winSize, int bufSize, sockaddr_in clientAddr): ChromaProtocol(winSize, bufSize) 
{
    this->clientAddr = clientAddr;
    timers = std::vector<Timer>(bufSize);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

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

    vector<char> buffer(chunkSize);

    while (base <= getNextSeqNum() && !file.eof())
    {
        while ((getNextSeqNum() < base + windowSize) && (file.read(buffer.data(), chunkSize) || file.gcount() > 0)) {

            vector<char> actualData(buffer.begin(), buffer.begin() + file.gcount());

            Packet pkt(getNextSeqNum(), actualData, ChromaMethod::POST, addr);
            sendBuffer[pkt.seqNum] = pkt;

            cout << "Servidor enviou pacote " << pkt.seqNum << " (" << file.gcount() << " bytes)" << endl;
            
            setTimerAndSendPacket(pkt, 50, clientAddr);
            nextSeqNum++;
            this_thread::sleep_for(chrono::milliseconds(1));
        }

        receiveData();

    }

    file.close();

    Packet pkt(-1, vector<char>(), ChromaMethod::ACK, addr);
    sendPacket(pkt, clientAddr);

    cout << "Envio concluído: " << filename << endl;
}

// Receber ACKs/NACKs do cliente
void ChromaServer::receiveData() {

    Packet pkt;

    while (recvPacket(pkt) > 0) {
        if (pkt.method == ChromaMethod::ACK && !isCorrupted(pkt)) {
            cout << "ACK recebido para seq: " << pkt.seqNum << endl;
            
            timers[pkt.seqNum].stop();
            sendBuffer[pkt.seqNum].received = true;

            // Se o ACK for para a base da janela, tenta deslizar a janela
            if (pkt.seqNum == base) {
                while (base < bufferSize && sendBuffer[base].received) {
                    base++;
                }
            }
        }
        else if (pkt.method == ChromaMethod::NACK) {
            cout << "NACK recebido para seq: " << pkt.seqNum << endl;
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

