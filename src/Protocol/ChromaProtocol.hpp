#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream> 
#include <arpa/inet.h>
#include <fstream>

using namespace std;

enum class ChromaMethod {
    UNKNOWN,
    GET,
    POST,
    ACK,
    NACK    
};

class Packet {
public:
    vector<char> data;
    
    int seqNum;
    ChromaMethod method;
    bool received = false;
    string sha256;

    sockaddr_in srcAddr;

    Packet() : seqNum(0), method(ChromaMethod::UNKNOWN), sha256("") {}
    Packet(int seq, const vector<char>& d, ChromaMethod m, sockaddr_in& src): seqNum(seq), data(d), method(m), srcAddr(src), sha256("") {  sha256 = makeCheckSum(d);}

    string makeCheckSum(const vector<char>& data) const {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) throw runtime_error("Falha ao criar contexto EVP");

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
            EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            throw runtime_error("Falha ao calcular SHA256");
        }

        EVP_MD_CTX_free(ctx);

        stringstream ss;
        for (unsigned int i = 0; i < hashLen; i++) {
            ss << hex << setw(2) << setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    string toString() const {
        return to_string(seqNum) + "|" +
               to_string(static_cast<int>(method)) + "|" +
               sha256 + "|" +
               dataToString(data);
    }

    void fromString(const std::string& str, const sockaddr_in& src) {
        
        srcAddr = src;

        size_t pos1 = str.find('|');
        size_t pos2 = str.find('|', pos1 + 1);
        size_t pos3 = str.find('|', pos2 + 1);

        seqNum = std::stoi(str.substr(0, pos1));
        method = static_cast<ChromaMethod>(std::stoi(str.substr(pos1 + 1, pos2 - pos1 - 1)));
        sha256 = str.substr(pos2 + 1, pos3 - pos2 - 1);
        std::string dataStr = str.substr(pos3 + 1);
        data = stringToData(dataStr);
    }

    static string dataToString(const std::vector<char>& data) {
        ostringstream oss;
        for (unsigned char c : data) {
            oss << hex << setw(2) << setfill('0') << (int)c;
        }
        return oss.str();
    }

    static vector<char> stringToData(const string& s) {
        vector<char> result;
        for (size_t i = 0; i < s.length(); i += 2) {
            string byteStr = s.substr(i, 2);
            char byte = static_cast<char>(stoi(byteStr, nullptr, 16));
            result.push_back(byte);
        }
        return result;
    }

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

    ChromaProtocol(int winSize, int bufSize);
    virtual ~ChromaProtocol();

    ssize_t sendPacket(const Packet& pkt, const sockaddr_in& dest);
    ssize_t recvPacket(Packet& pkt);

    bool isCorrupted(const Packet& pkt);
    int getNextSeqNum() const { return nextSeqNum % bufferSize; }
    bool waitResponse(int timeoutSec);

    virtual void sendData(const char* data, size_t len) = 0;
    virtual void receiveData() = 0;

    void sendConfirmation(int seqNum, ChromaMethod method, const sockaddr_in& dest) {
        Packet pkt(seqNum, vector<char>(), method, addr);
        sendPacket(pkt, dest);
    }
};