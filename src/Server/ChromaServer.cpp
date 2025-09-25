#include "ChromaServer.hpp"
#include <fstream>
#include <fcntl.h>
#include <cmath>
#include <iostream>

using namespace std;

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
         << " | IP: " << inet_ntoa(addr.sin_addr) << endl;
}

ChromaServer::~ChromaServer() {
    cout << "[ChromaServer] Encerrando servidor, limpando timers..." << endl;
    timers.clear();
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    if (chunkSize == 0 || chunkSize > CHROMA_MAX_DATA) {
        cerr << "[ChromaServer] chunkSize inválido. Ajustando para " << CHROMA_MAX_DATA << endl;
        chunkSize = CHROMA_MAX_DATA;
    }

    ifstream file(filename, ios::in | ios::binary);
    if (!file.is_open()) {
        cerr << "[ChromaServer] Erro ao abrir arquivo: " << filename << endl;
        Packet nack(0, {}, ChromaFlag::NACK, addr);
        sendPacket(nack, clientAddr);
        return;
    }

    Packet meta = makeMetaDataPacket(filename, file, chunkSize);
    sendPacket(meta, clientAddr);

    if (!waitResponse(5)) {
        cerr << "[ChromaServer] Timeout aguardando ACK de metadados." << endl;
        return;
    }

    Packet ackMeta;
    if (recvPacket(ackMeta) <= 0 || ackMeta.flag != ChromaFlag::ACK || isCorrupted(ackMeta)) {
        cerr << "[ChromaServer] Falha ao receber ACK válido de metadados." << endl;
        return;
    }

    bool finishedReading = false;
    while (!finishedReading || base < nextSeqNum) {
        while ((uint8_t)(nextSeqNum - base) < windowSize && !finishedReading) {
            vector<char> buffer(chunkSize);
            file.read(buffer.data(), chunkSize);
            streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                buffer.resize(bytesRead);
                Packet pkt(static_cast<uint8_t>(nextSeqNum), buffer, ChromaFlag::DATA, addr);

                bufferPackets[pkt.seqNum] = pkt;

                cout << "[ChromaServer] Enviando pacote "
                     << static_cast<int>(pkt.seqNum) << " (" << bytesRead << " bytes)" << endl;

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
    cout << "[ChromaServer] Arquivo enviado com sucesso!" << endl;
}

void ChromaServer::receiveData() {
    Packet pkt;
    while (recvPacket(pkt) > 0) {
        if (isCorrupted(pkt)) {
            cerr << "[ChromaServer] Pacote corrompido ignorado." << endl;
            continue;
        }

        if (pkt.flag == ChromaFlag::ACK) {
            int seq = static_cast<int>(pkt.seqNum);
            cout << "[ChromaServer] ACK recebido para seq " << seq << endl;

            if (timers.count(pkt.seqNum)) {
                timers[pkt.seqNum].stop();
                timers.erase(pkt.seqNum);
            }

            if (pkt.seqNum == base) {
                while (bufferPackets.count(base)) {
                    bufferPackets.erase(base);
                    base++;
                }
            }
        } else if (pkt.flag == ChromaFlag::NACK) {
            cout << "[ChromaServer] NACK recebido para seq "
                 << static_cast<int>(pkt.seqNum) << endl;
        }
    }
}

void ChromaServer::setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest) {
    if (!timers.count(pkt.seqNum)) {
        timers.emplace(pkt.seqNum, Timer{});
        timers[pkt.seqNum].start(timeoutMs, [this, pkt, dest]() {
            cout << "[ChromaServer] Timeout seq "
                 << static_cast<int>(pkt.seqNum) << " → retransmitindo" << endl;
            sendPacket(pkt, dest);
        });
    }
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
