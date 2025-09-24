#pragma once

#include "../Protocol/ChromaProtocol.hpp"
#include <string>

class ChromaClient : public ChromaProtocol {
private:
    sockaddr_in serverAddr{};
    sockaddr_in serverResponseAddr{};
    bool connected = false;
    std::string extensionFile = "";
    std::string filename = "";
    long long fileSize = 0;
    int totalPackets = 0;

    bool quietMode = false;          

public:
    ChromaClient(int winSize, int bufSize);
    ~ChromaClient();

    void sendData(const char* data, size_t len) override;

    void receiveData() override;

    void connectToServer(const char* ip, int port);

    void disconnect();

    bool isConnected() const { return connected; }

    void setQuietMode(bool quiet);

    void readFileMetadata(const Packet& pkt);
};
