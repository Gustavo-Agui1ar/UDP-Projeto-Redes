#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream> 

using namespace std;

enum class ChromaMethod {
    UNKNOWN,
    GET,
    POST,
    ACK,
    NACK    
};

class Packet {
public:
    int seqNum;
    vector<char> data;
    ChromaMethod method;
    bool received = false;
    string checksum;

    sockaddr_in srcAddr;

    Packet() : seqNum(0), method(ChromaMethod::UNKNOWN), checksum("") {}
    Packet(int seq, const vector<char>& d, ChromaMethod m, sockaddr_in& src): seqNum(seq), data(d), method(m), srcAddr(src), checksum("") {  checksum = makeCheckSum(d);}

    string makeCheckSum(const vector<char>& data) const {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) throw runtime_error("Falha ao criar contexto EVP");

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
            EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            throw runtime_error("Falha ao calcular SHA256");
        }

        EVP_MD_CTX_free(ctx);

        stringstream ss;
        for (unsigned int i = 0; i < hashLen; i++) {
            ss << hex << setw(2) << setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

};

class ChromaProtocol {
protected:
    int sockfd;
    sockaddr_in addr;

    int windowSize;      
    int bufferSize;
    int base;
    int nextSeqNum;

    vector<Packet> sendBuffer;

public:

    ChromaProtocol(int winSize, int bufSize);
    virtual ~ChromaProtocol();

    ssize_t sendPacket(const Packet& pkt, const sockaddr_in& dest);
    ssize_t recvPacket(Packet& pkt);

    bool isCorrupted(const Packet& pkt);
    int getNextSeqNum() const { return nextSeqNum % bufferSize; }
    bool waitResponse(int timeoutSec);

    virtual void sendData(const char* data, size_t len) = 0;
    virtual void receiveData() = 0;
};