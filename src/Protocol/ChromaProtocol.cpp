#include "ChromaProtocol.hpp"

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
    
    string jsonStr = pkt.toString();

    return sendto(sockfd, jsonStr.data(), jsonStr.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
}

ssize_t ChromaProtocol::recvPacket(Packet& pkt) {
    socklen_t addrLen = sizeof(pkt.srcAddr);
    char buffer[1024];

    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&pkt.srcAddr, &addrLen);
    
    if (n <= 0) return n;

    pkt.fromString(string(buffer, n), pkt.srcAddr);

    return n;
}

bool ChromaProtocol::isCorrupted(const Packet& pkt) {

    string sum = pkt.makeCheckSum(pkt.data);

    return sum.compare(pkt.sha256) != 0;
}

bool ChromaProtocol::waitResponse(int timeoutSec) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;

    int ret = select(sockfd + 1, &fds, nullptr, nullptr, &tv);
    return ret > 0;
}