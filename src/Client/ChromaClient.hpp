#pragma once

#include "../Protocol/ChromaProtocol.hpp"

class ChromaClient : public ChromaProtocol
{
private:

    sockaddr_in serverAddr;
    sockaddr_in serverResponseAddr;
    bool connected = false;

public:

    ChromaClient(int winSize, int bufSize);
    ~ChromaClient();    

    void sendData(const char* data, size_t len) override;
    void receiveData() override;

    void connectToServer(const char* ip, int port);
    void disconnect();
    bool isConnected() const { return connected; }

    void recreateFile(const char* filename);
};

