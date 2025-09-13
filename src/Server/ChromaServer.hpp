#pragma once
#include "ChromaProtocol.hpp"
#include <iostream>
#include <fstream>
#include <thread>

class ChromaServer : public ChromaProtocol {
public:
    ChromaServer(int winSize, int bufSize, sockaddr_in clientAddr, int sockfd);

    void sendData(const char* filename, size_t chunkSize = 512);
    void receiveData() override;

private:
    sockaddr_in clientAddr{};
};
