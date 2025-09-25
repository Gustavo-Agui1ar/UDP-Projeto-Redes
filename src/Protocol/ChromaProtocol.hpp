#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cerrno>

#define WINDOW_SIZE 127
#define BUFFER_SIZE 256

constexpr size_t UDP_MAX_PAYLOAD = 1472;      // 1500 - 20 (IP) - 8 (UDP)
constexpr size_t CHROMA_HEADER_SIZE = 10;     // seq(1) + flag(1) + dsize(4) + checksum(4)
constexpr size_t CHROMA_MAX_DATA = UDP_MAX_PAYLOAD - CHROMA_HEADER_SIZE;

enum class ChromaFlag : uint8_t {
    UNKNOWN = 0,
    GET,
    DATA,
    ACK,
    NACK,
    END,
    META
};

class Packet {
public:
    uint8_t seqNum{0};
    ChromaFlag flag{ChromaFlag::UNKNOWN};
    uint32_t checksum{0};             
    std::vector<char> data;
    sockaddr_in srcAddr{};

    Packet() = default;

    Packet(uint8_t seq, const std::vector<char>& d, ChromaFlag f, sockaddr_in src = {})
        : seqNum(seq), flag(f), data(d), srcAddr(src) {
        checksum = computeChecksum(data);
        std::memset(&srcAddr, 0, sizeof(srcAddr));
    }

    [[nodiscard]] std::vector<char> serialize() const {
        std::vector<char> buffer;

        // seqNum (1 byte)
        buffer.push_back(static_cast<char>(seqNum));

        // flag (1 byte)
        buffer.push_back(static_cast<char>(flag));

        // tamanho dos dados (4 bytes)
        uint32_t dsize = htonl(static_cast<uint32_t>(data.size()));
        appendToBuffer(buffer, &dsize, sizeof(dsize));

        // checksum (4 bytes)
        uint32_t chk = htonl(checksum);
        appendToBuffer(buffer, &chk, sizeof(chk));

        // dados
        buffer.insert(buffer.end(), data.begin(), data.end());

        return buffer;
    }

    void deserialize(const std::vector<char>& buffer, const sockaddr_in& src) {
        srcAddr = src; // Store source address
        
        size_t offset = 0;

        constexpr size_t headerSize = 1 + 1 + sizeof(uint32_t) + sizeof(uint32_t);
        if (buffer.size() < headerSize) {
            throw std::runtime_error("Buffer menor que cabeçalho mínimo");
        }

        // seqNum
        seqNum = static_cast<uint8_t>(buffer[offset]);
        offset += sizeof(seqNum);

        // flag
        flag = static_cast<ChromaFlag>(static_cast<unsigned char>(buffer[offset]));
        offset += sizeof(flag);

        // tamanho dos dados
        uint32_t dsize_n{};
        std::memcpy(&dsize_n, buffer.data() + offset, sizeof(dsize_n));
        uint32_t dsize = ntohl(dsize_n);
        offset += sizeof(dsize_n);

        // checksum
        uint32_t chk_n{};
        std::memcpy(&chk_n, buffer.data() + offset, sizeof(chk_n));
        checksum = ntohl(chk_n);
        offset += sizeof(chk_n);

        if (buffer.size() < offset + dsize) {
            throw std::runtime_error("Buffer inconsistente: tamanho insuficiente");
        }

        // dados
        data.assign(buffer.begin() + offset, buffer.begin() + offset + dsize);
    }

    /// Calcula um CRC32 simples sobre os dados
    static uint32_t computeChecksum(const std::vector<char>& d) {
        uint32_t crc = 0xFFFFFFFF;
        for (unsigned char b : d) {
            crc ^= b;
            for (int i = 0; i < 8; ++i) {
                crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
            }
        }
        return ~crc;
    }

private:
    static void appendToBuffer(std::vector<char>& buffer, const void* data, size_t size) {
        const auto* ptr = reinterpret_cast<const char*>(data);
        buffer.insert(buffer.end(), ptr, ptr + size);
    }
};

class ChromaProtocol {
protected:
    int sockfd{-1};
    sockaddr_in addr{};

    uint8_t windowSize{0};
    uint8_t bufferSize{0};
    uint8_t base{0};
    uint8_t nextSeqNum{0};

    std::map<uint8_t, Packet> bufferPackets;

public:
    ChromaProtocol(int winSize);
    virtual ~ChromaProtocol();

    ssize_t sendPacket(const Packet& pkt, const sockaddr_in& dest);
    ssize_t recvPacket(Packet& pkt);

    bool isCorrupted(const Packet& pkt) const {
        return pkt.checksum != Packet::computeChecksum(pkt.data);
    }

    [[nodiscard]] uint8_t getNextSeqNum() const { return nextSeqNum; }
    [[nodiscard]] uint8_t getBase() const { return base; }
    [[nodiscard]] int getWindowSize() const { return windowSize; }
    
    bool waitResponse(int timeoutSec);

    void sendConfirmation(uint8_t seqNum, ChromaFlag flag, const sockaddr_in& dest) {
        Packet pkt(seqNum, {}, flag);
        if (sendPacket(pkt, dest) < 0) {
            std::cerr << "[ChromaProtocol] Erro ao enviar confirmação" << std::endl;
        }
    }

    bool isSeqInWindow(uint8_t seq, uint8_t base) const {
        return static_cast<uint8_t>(seq - base) < windowSize;
    }
    uint8_t getSeqDistance(uint8_t from, uint8_t to) const {
        return static_cast<uint8_t>(to - from);
    }

    virtual void sendData(const char* data, size_t len) = 0;
    virtual void receiveData() = 0;
};

