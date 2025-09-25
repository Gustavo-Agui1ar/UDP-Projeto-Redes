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
    int totalPackets = 0;

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

    void connectToServer(const char* ip, int port);
    void disconnect();

    bool isConnected() const { return connected; }
    void setQuietMode(bool quiet) { quietMode = quiet; }

    void readFileMetadata(const Packet& pkt);
};