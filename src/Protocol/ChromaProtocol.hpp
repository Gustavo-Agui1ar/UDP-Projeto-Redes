#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <stdexcept>

using namespace std;

enum class ChromaMethod {
    UNKNOWN,
    GET,
    POST,
    ACK,
    NACK    
};

struct Packet {
    int seqNum;
    vector<char> data;
    ChromaMethod method;
    int checksum;
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

    virtual void sendData(const char* data, size_t len) = 0;
    virtual void receiveData() = 0;
};