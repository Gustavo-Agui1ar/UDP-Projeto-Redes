#pragma once

#include "../Protocol/ChromaProtocol.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

class ChromaClient : public ChromaProtocol {
private:
    sockaddr_in serverAddr{};
    sockaddr_in serverResponseAddr{};
    bool connected = false;
    bool quietMode = false;

    std::string extensionFile = "";
    std::string filename = "";
    long long fileSize = 0;
    int packetsReceived = 0;
    int totalPackets = 0;

    int chanceLossPacket = 0; 

    void logMsg(const std::string& msg, const char* color = "\033[0m") const {
        if (!quietMode) std::cout << color << msg << "\033[0m" << std::endl;
    }
    void logErr(const std::string& msg, const char* color = "\033[31m") const {
        if (!quietMode) std::cerr << color << msg << "\033[0m" << std::endl;
    }

public:
    ChromaClient(int winSize);
    ~ChromaClient();

    void sendData(const char* data, size_t len) override;
    void receiveData() override;

    void setPacketLossChance(int chance) { 
        if (chance < 0) chance = 0;
        if (chance > 100) chance = 100;
        chanceLossPacket = chance; 
    }

    bool isPacketLost() const {
        if (chanceLossPacket <= 0) return false;
        int roll = rand() % 101;
        return roll < chanceLossPacket;
    }

    void connectToServer(const char* ip, int port);
    void disconnect();

    bool isConnected() const { return connected; }
    void setQuietMode(bool quiet) { quietMode = quiet; }

    void readFileMetadata(const Packet& pkt);
    void printProgress(long long bytesSent, long long fileSize, int packetsSent, int totalPackets);
};