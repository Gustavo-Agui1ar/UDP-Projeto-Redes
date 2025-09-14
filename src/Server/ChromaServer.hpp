#pragma once

#include "../Protocol/ChromaProtocol.hpp"
#include "../Protocol/Timer.hpp"
#include <fstream>

class ChromaServer : public ChromaProtocol {
public:

    ChromaServer(int winSize, int bufSize, sockaddr_in clientAddr);

    void sendData(const char* filename, size_t chunkSize = 512);
    void receiveData() override;

    void setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest);

private:
    sockaddr_in clientAddr{};
    vector<Timer> timers;
};
