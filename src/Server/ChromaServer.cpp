#include "ChromaServer.hpp"
#include <fstream>
#include <fcntl.h>
#include <cmath>
#include <iostream>

using namespace std;

#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"
#define ORANGE  "\033[38;5;208m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define RESET   "\033[0m"

ChromaServer::ChromaServer(int winSize, const sockaddr_in& clientAddr)
    : ChromaProtocol(winSize), clientAddr(clientAddr), scheduler()
{
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw runtime_error("Erro ao bindar socket do servidor");
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        throw runtime_error("Erro ao obter porta atribuída ao servidor");
    }

    cout << CYAN << "[ChromaServer] Rodando na porta "
         << ntohs(addr.sin_port)
         << " | IP: " << inet_ntoa(addr.sin_addr) 
         << RESET << "\n";
}

ChromaServer::~ChromaServer() {
    cout << CYAN << "[ChromaServer] Encerrando servidor, limpando timers..."
         << RESET << "\n";
    scheduler.stop();
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    if (chunkSize == 0 || chunkSize > CHROMA_MAX_DATA) {
        cerr << YELLOW << "[ChromaServer] chunkSize inválido. Ajustando para "
             << CHROMA_MAX_DATA << RESET << "\n";
        chunkSize = CHROMA_MAX_DATA;
    }

    ifstream file(filename, ios::in | ios::binary);
    if (!file.is_open()) {
        std::string errMsg = "erro ao abrir arquivo com nome incorreto ou inexistente";
        std::vector<char> errMsgVec(errMsg.begin(), errMsg.end());

        Packet nack(0, errMsgVec, ChromaFlag::NACK, addr);
        sendPacket(nack, clientAddr);

        cerr << RED << "[ChromaServer] " << errMsg << RESET << "\n";
        return;
    }

    Packet meta = makeMetaDataPacket(filename, file, chunkSize);
    sendPacket(meta, clientAddr);

    if (!waitResponse(5)) {
        cerr << RED << "[ChromaServer] Timeout aguardando ACK de metadados."
             << RESET << "\n";
        return;
    }

    Packet ackMeta;
    if (recvPacket(ackMeta) <= 0 || ackMeta.flag != ChromaFlag::ACK || isCorrupted(ackMeta)) {
        cerr << RED << "[ChromaServer] Falha ao receber ACK válido de metadados."
             << RESET << "\n";
        return;
    }

    bool finishedReading = false;
    while (!finishedReading || !bufferPackets.empty()) {
        while ((uint8_t)(nextSeqNum - base) < windowSize && !finishedReading) {
            vector<char> buffer(chunkSize);
            file.read(buffer.data(), chunkSize);
            streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                Packet pkt(static_cast<uint8_t>(nextSeqNum),
                           std::vector<char>(buffer.begin(), buffer.begin() + bytesRead),
                           ChromaFlag::DATA, addr);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bufferPackets[pkt.seqNum] = pkt;
                }

                cout << GREEN << "[ChromaServer] Enviando pacote "
                     << static_cast<int>(pkt.seqNum)
                     << " (" << bytesRead << " bytes)"
                     << RESET << "\n";

                setTimerAndSendPacket(pkt, 200, clientAddr);
                nextSeqNum++;
            }

            if (file.eof()) finishedReading = true;
        }

        receiveData();
    }

    file.close();

    // Pacote final com flag de encerramento
    Packet endPkt(0, {}, ChromaFlag::END, addr);
    sendPacket(endPkt, clientAddr);
    cout << BLUE << "[ChromaServer] Arquivo enviado com sucesso!" 
         << RESET << "\n";
}

void ChromaServer::receiveData() {
    Packet pkt;

    while (true) {
        int r = recvPacket(pkt);
        if (r <= 0) break; 

        if (isCorrupted(pkt)) {
            cerr << RED << "[ChromaServer] Pacote corrompido ignorado."
                 << RESET << "\n";
            continue;
        }

        if (pkt.flag == ChromaFlag::ACK) {
            std::lock_guard<std::mutex> lock(m_mutex);

            uint8_t seq = pkt.seqNum;
            cerr << YELLOW << "[ChromaServer] ACK recebido para seq "
                << (int)seq << RESET << "\n";

            scheduler.cancel(seq);   
            size_t erased = bufferPackets.erase(seq);

            uint8_t oldBase = base;
            while (base != nextSeqNum && bufferPackets.find(base) == bufferPackets.end()) {
                base = static_cast<uint8_t>((base + 1) % 256);
            }
            if (oldBase != base) {
                cerr << "[DEBUG] base avançou de " << (int)oldBase 
                    << " para " << (int)base << "\n";
            }
        } 
        else if (pkt.flag == ChromaFlag::NACK) {
            cout << ORANGE << "[ChromaServer] NACK recebido para seq "
                << static_cast<int>(pkt.seqNum) << RESET << "\n";
        }

    }
}

void ChromaServer::setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest) {
    uint8_t seq = pkt.seqNum;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        bufferPackets[seq] = pkt;
    }

    auto callback = [this, seq, dest]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (bufferPackets.count(seq)) {
            cerr << MAGENTA << "[ChromaServer] Timeout -> retransmitindo seq " << (int)seq << RESET << "\n";
            sendPacket(bufferPackets[seq], dest);
        }
    };

    scheduler.addRepeatingTimeout(seq, timeoutMs, callback);

    sendPacket(pkt, dest);
}


Packet ChromaServer::makeMetaDataPacket(const string& filename, ifstream& file, size_t chunkSize) {
    string pathStr(filename);
    size_t lastSlash = pathStr.find_last_of("/\\");
    string shortFilename = (lastSlash == string::npos) ? pathStr : pathStr.substr(lastSlash + 1);

    string extension = "bin";
    size_t lastDot = shortFilename.find_last_of(".");
    if (lastDot != string::npos) {
        extension = shortFilename.substr(lastDot + 1);
    }

    file.seekg(0, ios::end);
    long long fileSize = file.tellg();
    file.seekg(0, ios::beg);

    int totalPackets = static_cast<int>(ceil((double)fileSize / chunkSize));

    vector<char> meta;
    auto appendStr = [&](const string& s) {
        meta.insert(meta.end(), s.begin(), s.end());
        meta.push_back('\0');
    };

    appendStr(shortFilename);
    appendStr(extension);
    appendStr(to_string(fileSize));
    appendStr(to_string(totalPackets));

    return Packet(0, meta, ChromaFlag::META, addr);
}
