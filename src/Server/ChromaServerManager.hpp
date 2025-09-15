#pragma once

#include "../Protocol/ChromaProtocol.hpp"

class ChromaServerManager : public ChromaProtocol
{
private:
    sockaddr_in serverAddr;
    bool running = false;
    int serverPort;
    int limitConnections;

public:
    ChromaServerManager(int winSize, int bufSize, int port = 8080);
    ~ChromaServerManager();

    void start();
    void CreateServer(const char* ip, Packet pkt);
    void StopServer();
    bool isRunning() const { return running; }
    void sendData(const char* data, size_t len) override {}
    void receiveData() override {}
};