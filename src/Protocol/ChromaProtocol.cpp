#include "ChromaProtocol.hpp"
#include <iostream>

using namespace std;

ChromaProtocol::ChromaProtocol(int winSize, int bufSize) : windowSize(winSize), bufferSize(bufSize), base(0), nextSeqNum(0) 
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) throw runtime_error("Erro ao criar socket");

    cout << "Socket criado com sucesso: " << sockfd << endl;

    memset(&addr, 0, sizeof(addr));
    sendBuffer = vector<Packet>(bufferSize);
}

ChromaProtocol::~ChromaProtocol() {
    close(sockfd);
}

ssize_t ChromaProtocol::sendPacket(const Packet& pkt, const sockaddr_in& dest) {
    return sendto(sockfd, pkt.data.data(), pkt.data.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
}

ssize_t ChromaProtocol::recvPacket(Packet& pkt) {
    socklen_t addrLen = sizeof(pkt.srcAddr);
    char buffer[1024];

    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&pkt.srcAddr, &addrLen);
    
    if (n > 0) {
        pkt.data.assign(buffer, buffer + n);
    }
    return n;
}

bool ChromaProtocol::isCorrupted(const Packet& pkt) {

    string sum = pkt.makeCheckSum(pkt.data);

    return sum.compare(pkt.checksum) != 0;
}
