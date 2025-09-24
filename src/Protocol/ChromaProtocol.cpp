#include "ChromaProtocol.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

ChromaProtocol::ChromaProtocol(int winSize, int bufSize) 
    : windowSize(winSize), bufferSize(bufSize), base(0), nextSeqNum(0), sendBuffer(bufSize) 
{
    sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Erro ao criar socket: " + std::string(std::strerror(errno)));
    }

    std::cout << "[ChromaProtocol] Socket criado com sucesso (fd=" << sockfd << ")\n";
    std::memset(&addr, 0, sizeof(addr));
}

ChromaProtocol::~ChromaProtocol() {
    if (sockfd >= 0) {
        ::close(sockfd);
        std::cout << "[ChromaProtocol] Socket fechado (fd=" << sockfd << ")\n";
    }
}

ssize_t ChromaProtocol::sendPacket(const Packet& pkt, const sockaddr_in& dest) {
    auto buffer = pkt.serialize();
    ssize_t sent = ::sendto(sockfd, buffer.data(), buffer.size(), 0,
                            reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
    if (sent < 0) {
        std::cerr << "[ChromaProtocol] Erro em sendto(): " << std::strerror(errno) << "\n";
    }
    return sent;
}

ssize_t ChromaProtocol::recvPacket(Packet& pkt) {
    std::vector<char> buffer(UDP_MAX_PAYLOAD);
    socklen_t addrLen = sizeof(pkt.srcAddr);

    ssize_t received = ::recvfrom(sockfd, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&pkt.srcAddr), &addrLen);
    if (received <= 0) {
        if (received < 0) {
            std::cerr << "[ChromaProtocol] Erro em recvfrom(): " << std::strerror(errno) << "\n";
        }
        return received;
    }

    try {
        pkt.deserialize({buffer.begin(), buffer.begin() + received}, pkt.srcAddr);
    } catch (const std::runtime_error& e) {
        std::cerr << "[ChromaProtocol] Falha ao desserializar pacote: " << e.what() << " (bytes recebidos=" << received << ")\n";
        return -1;
    }

    return received;
}

bool ChromaProtocol::waitResponse(int timeoutSec) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    timeval tv{timeoutSec, 0};
    int ret = ::select(sockfd + 1, &fds, nullptr, nullptr, &tv);

    if (ret < 0) {
        throw std::runtime_error("Erro em select(): " + std::string(std::strerror(errno)));
    }
    return ret > 0;
}
