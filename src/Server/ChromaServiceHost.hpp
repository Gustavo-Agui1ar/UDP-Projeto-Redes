#pragma once

#include "../Protocol/ChromaProtocol.hpp"

class ChromaServiceHost : public ChromaProtocol
{
private:
    sockaddr_in serverAddr;
    bool running = false;
    int serverPort;
    int limitConnections;

public:
    ChromaServiceHost(int winSize, int bufSize, int port = 8080);
    ~ChromaServiceHost();

    void start();
    void CreateServer(const char* ip, Packet pkt);
    void StopServer();
    bool isRunning() const { return running; }
    void sendData(const char* data, size_t len) override {}
    void receiveData() override {}
};