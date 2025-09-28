#include "ChromaServer.hpp"
#include <fstream>
#include <fcntl.h>
#include <cmath>
#include <iostream>

using namespace std;

Timer ChromaServer::scheduler;

ChromaServer::ChromaServer(int winSize, const sockaddr_in& clientAddr)
    : ChromaProtocol(winSize), clientAddr(clientAddr)
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

    cout << "[ChromaServer] Rodando na porta " << ntohs(addr.sin_port)
         << " | IP: " << inet_ntoa(addr.sin_addr) << "\n";
}

ChromaServer::~ChromaServer() {
    cout << "[ChromaServer] Encerrando servidor, limpando timers..." << "\n";
    timerHandles.clear();
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    if (chunkSize == 0 || chunkSize > CHROMA_MAX_DATA) {
        cerr << "[ChromaServer] chunkSize inválido. Ajustando para " << CHROMA_MAX_DATA << "\n";
        chunkSize = CHROMA_MAX_DATA;
    }

    ifstream file(filename, ios::in | ios::binary);
    if (!file.is_open()) {
        cerr << "[ChromaServer] Erro ao abrir arquivo: " << filename << "\n";
        Packet nack(0, {}, ChromaFlag::NACK, addr);
        sendPacket(nack, clientAddr);
        return;
    }

    Packet meta = makeMetaDataPacket(filename, file, chunkSize);
    sendPacket(meta, clientAddr);

    if (!waitResponse(5)) {
        cerr << "[ChromaServer] Timeout aguardando ACK de metadados." << "\n";
        return;
    }

    Packet ackMeta;
    if (recvPacket(ackMeta) <= 0 || ackMeta.flag != ChromaFlag::ACK || isCorrupted(ackMeta)) {
        cerr << "[ChromaServer] Falha ao receber ACK válido de metadados." << "\n";
        return;
    }

    bool finishedReading = false;
    while (!finishedReading || !bufferPackets.empty()) {
        while ((uint8_t)(nextSeqNum - base) < windowSize && !finishedReading) {
            vector<char> buffer(chunkSize);
            file.read(buffer.data(), chunkSize);
            streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                Packet pkt(static_cast<uint8_t>(nextSeqNum), std::vector<char>(buffer.begin(), buffer.begin() + bytesRead), ChromaFlag::DATA, addr);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bufferPackets[pkt.seqNum] = pkt;
                }

                cout << "[ChromaServer] Enviando pacote "
                     << static_cast<int>(pkt.seqNum) << " (" << bytesRead << " bytes)\n";

                setTimerAndSendPacket(pkt, 200, clientAddr);
                nextSeqNum++;
            }

            if (file.eof()) finishedReading = true;
        }

        receiveData();
        processRetransmissions(200);
    }

    file.close();

    // Pacote final com flag de encerramento
    Packet endPkt(0, {}, ChromaFlag::END, addr);
    sendPacket(endPkt, clientAddr);
    cout << "[ChromaServer] Arquivo enviado com sucesso!" << "\n";
}

void ChromaServer::receiveData() {
    Packet pkt;

    while (true) {
        int r = recvPacket(pkt);
        if (r <= 0) break; 

        if (isCorrupted(pkt)) {
            std::cerr << "[ChromaServer] Pacote corrompido ignorado.\n";
            continue;
        }

        if (pkt.flag == ChromaFlag::ACK) {
            std::lock_guard<std::mutex> lock(m_mutex);

            uint8_t seq = pkt.seqNum;
            std::cerr << "[ChromaServer] ACK recebido para seq " << (int)seq << "\n";

            auto itTimer = timerHandles.find(seq);
            if (itTimer != timerHandles.end()) {
                scheduler.cancel(itTimer->second);
                timerHandles.erase(itTimer);
            }

            bufferPackets.erase(seq);

            while (base != nextSeqNum && bufferPackets.find(base) == bufferPackets.end()) {
                uint8_t old = base;
                base = static_cast<uint8_t>((base + 1) % 256);
            }
        } 
        else if (pkt.flag == ChromaFlag::NACK) {
            std::cout << "[ChromaServer] NACK recebido para seq "
                      << static_cast<int>(pkt.seqNum) << "\n";
        }
    }
}




void ChromaServer::setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest) {
    uint8_t seq = pkt.seqNum;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (timerHandles.count(seq)) {
            scheduler.cancel(timerHandles[seq]);
            timerHandles.erase(seq);
        }

        auto callback = [this, seq, dest]() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (bufferPackets.count(seq)) {
                retransmitQueue.push({seq, dest});
            }
        };

        timerHandles[seq] = scheduler.addTimeout(timeoutMs, callback);
        bufferPackets[seq] = pkt; 
    }

    sendPacket(pkt, dest);
}

void ChromaServer::processRetransmissions(int timeoutMs) {
    std::queue<TimeoutEvent> localQueue;

    {  
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(localQueue, retransmitQueue);
    }

    while (!localQueue.empty()) {
        TimeoutEvent ev = localQueue.front();
        localQueue.pop();

        Packet pkt;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!bufferPackets.count(ev.seq)) continue; 
            pkt = bufferPackets.at(ev.seq);
        }

        std::cerr << "[ChromaServer] Timeout -> retransmitindo seq " << (int)ev.seq << "\n";
        sendPacket(pkt, ev.dest);

        setTimerAndSendPacket(pkt, timeoutMs, ev.dest);
    }
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