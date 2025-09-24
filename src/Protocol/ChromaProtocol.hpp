#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <sys/socket.h>
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

constexpr size_t UDP_MAX_PAYLOAD = 1472;      // 1500 - 20 (IP) - 8 (UDP)
constexpr size_t CHROMA_HEADER_SIZE = 13;     // seq(4) + flag(1) + dsize(4) + checksum(4)
constexpr size_t CHROMA_MAX_DATA = UDP_MAX_PAYLOAD - CHROMA_HEADER_SIZE;

enum class ChromaFlag : uint8_t {
    UNKNOWN = 0,
    GET,
    DATA,
    ACK,
    NACK
};

class Packet {
public:
    int seqNum{0};
    ChromaFlag flag{ChromaFlag::UNKNOWN};
    bool received{false};
    uint32_t checksum{0};             
    std::vector<char> data;
    sockaddr_in srcAddr{};

    Packet() = default;

    Packet(int seq, const std::vector<char>& d, ChromaFlag f, const sockaddr_in& src)
        : seqNum(seq), flag(f), data(d), srcAddr(src) {
        checksum = computeChecksum(data);
    }

    [[nodiscard]] std::vector<char> serialize() const {// tamanho total do header = 13 bytes
        std::vector<char> buffer;

        // seqNum (4 bytes)
        uint32_t seq = htonl(static_cast<uint32_t>(seqNum));
        appendToBuffer(buffer, &seq, sizeof(seq));

        // flag (1 byte)
        buffer.push_back(static_cast<char>(flag));

        // tamanho dos dados (4 bytes)
        uint32_t dsize = htonl(static_cast<uint32_t>(data.size()));
        appendToBuffer(buffer, &dsize, sizeof(dsize));

        // checksum (4 bytes)
        uint32_t chk = htonl(checksum);
        appendToBuffer(buffer, &chk, sizeof(chk));

        // dados (entre 0 e CHROMA_MAX_DATA bytes)
        buffer.insert(buffer.end(), data.begin(), data.end());

        return buffer;
    }

    void deserialize(const std::vector<char>& buffer, const sockaddr_in& src) {
        srcAddr = src;
        size_t offset = 0;

        constexpr size_t headerSize = sizeof(uint32_t) + 1 + sizeof(uint32_t) + sizeof(uint32_t);
        if (buffer.size() < headerSize) {
            throw std::runtime_error("Buffer menor que cabeçalho mínimo");
        }

        // seqNum
        uint32_t seq_n{};
        std::memcpy(&seq_n, buffer.data() + offset, sizeof(seq_n));
        seqNum = ntohl(seq_n);
        offset += sizeof(seq_n);

        // flag
        flag = static_cast<ChromaFlag>(static_cast<unsigned char>(buffer[offset++]));

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

    int windowSize{0};
    int bufferSize{0};
    int base{0};
    int nextSeqNum{0};

    std::vector<Packet> sendBuffer;

public:
    ChromaProtocol(int winSize, int bufSize);
    virtual ~ChromaProtocol();

    ssize_t sendPacket(const Packet& pkt, const sockaddr_in& dest);
    ssize_t recvPacket(Packet& pkt);

    bool isCorrupted(const Packet& pkt) const {
        return pkt.checksum != Packet::computeChecksum(pkt.data);
    }

    [[nodiscard]] int getNextSeqNum() const { return nextSeqNum % bufferSize; }
    bool waitResponse(int timeoutSec);

    virtual void sendData(const char* data, size_t len) = 0;
    virtual void receiveData() = 0;

    void sendConfirmation(int seqNum, ChromaFlag flag, const sockaddr_in& dest) {
        Packet pkt(seqNum, {}, flag, addr);
        sendPacket(pkt, dest);
    }
};
