#pragma once

#include "../Protocol/ChromaProtocol.hpp"
#include "../Protocol/Timer.hpp"

#include <fstream>
#include <string>
#include <vector>

class ChromaServer : public ChromaProtocol {
public:
    ChromaServer(int winSize, const sockaddr_in& clientAddr);
    ~ChromaServer();

    void sendData(const char* filename, size_t chunkSize = 512);

    void receiveData() override;

    Packet makeMetaDataPacket(const std::string& filename, std::ifstream& file, size_t chunkSize);

    void setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest);

private:
    sockaddr_in clientAddr{};    
    static Timer scheduler;
    std::unordered_map<uint8_t, Timer::Id> timerHandles;
};
