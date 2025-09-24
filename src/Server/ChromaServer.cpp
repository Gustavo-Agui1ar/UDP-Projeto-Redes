#include "ChromaServer.hpp"
#include <fstream>
#include <fcntl.h>
#include <cmath>

using namespace std;

ChromaServer::ChromaServer(int winSize, int bufSize, const sockaddr_in& clientAddr)
    : ChromaProtocol(winSize, bufSize), clientAddr(clientAddr), timers(bufSize) 
{
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    // Coloca socket em modo não-bloqueante
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw runtime_error("Erro ao bindar socket do servidor");
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        throw runtime_error("Erro ao obter porta atribuída ao servidor");
    }

    cout << "Servidor rodando na porta: " << ntohs(addr.sin_port)
         << " | IP: " << inet_ntoa(addr.sin_addr) << endl;
}

ChromaServer::~ChromaServer() {
    cout << "Encerrando o servidor. Parando todos os timers..." << endl;
    for (auto& timer : timers) {
        timer.stop();
    }
}

void ChromaServer::sendData(const char* filename, size_t chunkSize) {
    
    if (chunkSize == 0) {
        chunkSize = CHROMA_MAX_DATA;
    }
    if (chunkSize > CHROMA_MAX_DATA) {
        cerr << "[ChromaServer] Aviso: chunkSize (" << chunkSize << ") maior que CHROMA_MAX_DATA ("
             << CHROMA_MAX_DATA << "). Ajustando para " << CHROMA_MAX_DATA << "." << endl;
        chunkSize = CHROMA_MAX_DATA;
    }
    
    ifstream file(filename, ios::in | ios::binary);
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo: " << filename << endl;
        Packet nack(0, {}, ChromaFlag::NACK, addr);
        sendPacket(nack, clientAddr);
        return;
    }

    sendPacket(makeMetaDataPacket(filename, file, chunkSize), clientAddr);

    if (!waitResponse(5)) {
        cerr << "Timeout esperando ACK do cliente para metadados." << endl;
        return;
    }

    Packet ackMeta;
    if (recvPacket(ackMeta) <= 0 || ackMeta.flag != ChromaFlag::ACK || isCorrupted(ackMeta)) {
        cerr << "Falha ao receber ACK válido do cliente." << endl;
        return;
    }

    bool finishedReading = false;
    while (!finishedReading || base < nextSeqNum) {
        while (nextSeqNum < base + windowSize && !finishedReading) {
            vector<char> buffer(chunkSize);
            file.read(buffer.data(), chunkSize);
            streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                buffer.resize(bytesRead);
                int idx = nextSeqNum % bufferSize;

                Packet pkt(nextSeqNum, buffer, ChromaFlag::DATA, addr);
                sendBuffer[idx] = pkt;
                sendBuffer[idx].received = false;

                cout << "Servidor enviou pacote " << pkt.seqNum << " (" << bytesRead << " bytes)" << endl;

                setTimerAndSendPacket(pkt, 50, clientAddr);
                nextSeqNum++;
            }

            if (file.eof()) {
                finishedReading = true;
            }
        }

        receiveData(); 
    }

    file.close();

    Packet endPkt(-1, {}, ChromaFlag::ACK, addr);
    sendPacket(endPkt, clientAddr);
    cout << "Arquivo enviado com sucesso!" << endl;
}

void ChromaServer::receiveData() {
    Packet pkt;
    while (recvPacket(pkt) > 0) {
        if (pkt.flag == ChromaFlag::ACK && !isCorrupted(pkt)) {
            int idx = pkt.seqNum % bufferSize;
            timers[idx].stop();
            sendBuffer[idx].received = true;

            cout << "ACK recebido para seq: " << pkt.seqNum << endl;

            if (pkt.seqNum == base) {
                while (base < nextSeqNum && sendBuffer[base % bufferSize].received) {
                    base++;
                }
            }
        } else if (pkt.flag == ChromaFlag::NACK) {
            cout << "NACK recebido para seq: " << pkt.seqNum << endl;
        }
    }
}

void ChromaServer::setTimerAndSendPacket(const Packet& pkt, int timeoutMs, const sockaddr_in& dest) {
    int idx = pkt.seqNum % bufferSize;

    if (idx < 0 || idx >= static_cast<int>(timers.size())) {
        cerr << "Índice inválido para timer (seq=" << pkt.seqNum << ")" << endl;
        sendPacket(pkt, dest);
        return;
    }

    timers[idx].start(timeoutMs, [this, pkt, dest]() {
        cout << "Timeout seq " << pkt.seqNum << " -> retransmitindo" << endl;
        sendPacket(pkt, dest);
    });

    sendPacket(pkt, dest);
}

Packet ChromaServer::makeMetaDataPacket(const string& filename, ifstream& file, size_t chunkSize) {
    // Nome curto do arquivo
    string pathStr(filename);
    size_t lastSlash = pathStr.find_last_of("/\\");
    string shortFilename = (lastSlash == string::npos) ? pathStr : pathStr.substr(lastSlash + 1);

    // Extensão
    string extension = "desconhecido";
    size_t lastDot = shortFilename.find_last_of(".");
    if (lastDot != string::npos) {
        extension = shortFilename.substr(lastDot);
    }

    // Tamanho do arquivo
    file.seekg(0, ios::end);
    long long fileSize = file.tellg();
    file.seekg(0, ios::beg);

    int totalPackets = static_cast<int>(ceil(static_cast<double>(fileSize) / chunkSize));

    vector<char> meta;
    auto appendStr = [&](const string& s) {
        meta.insert(meta.end(), s.begin(), s.end());
        meta.push_back('\0');
    };

    appendStr(shortFilename);
    appendStr(extension);
    appendStr(to_string(fileSize));
    appendStr(to_string(totalPackets));

    return Packet(0, meta, ChromaFlag::ACK, addr);
}
