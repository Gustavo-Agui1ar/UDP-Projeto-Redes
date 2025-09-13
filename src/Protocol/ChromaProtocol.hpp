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
    ACK,
    NACK    
};

struct Packet {
    int seqNum;
    vector<char> data;
    ChromaMethod method;
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
    ChromaProtocol(int winSize, int bufSize)
        : windowSize(winSize), bufferSize(bufSize),
          base(0), nextSeqNum(0) 
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) throw runtime_error("Erro ao criar socket");
        memset(&addr, 0, sizeof(addr));
    }

    virtual ~ChromaProtocol() {
        close(sockfd);
    }

    virtual void sendData(const char* data, size_t len) = 0;
    virtual void receiveData() = 0;
};