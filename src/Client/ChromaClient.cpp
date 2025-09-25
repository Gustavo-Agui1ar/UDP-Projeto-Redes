
#include "ChromaClient.hpp"

ChromaClient::ChromaClient(int winSize)
    : ChromaProtocol(winSize) {}

ChromaClient::~ChromaClient() {
    disconnect();
}

void ChromaClient::connectToServer(const char* ip, int port) {
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        throw std::runtime_error("Endereço inválido ou não suportado");
    }

    connected = true;
    logMsg("Cliente conectado ao servidor " + std::string(ip) + ":" + std::to_string(port), "\033[32m");
}

void ChromaClient::disconnect() {
    if (!connected) return;

    close(sockfd);
    connected = false;
    serverAddr = {};
    serverResponseAddr = {};

    logMsg("Cliente desconectado.", "\033[33m");
}

void ChromaClient::sendData(const char* data, size_t len) {
    std::string fileRequested(data, len);
    size_t pos = fileRequested.find_last_of('.');
    extensionFile = (pos != std::string::npos) ? fileRequested.substr(pos + 1) : "bin";

    logMsg("Solicitando arquivo: " + fileRequested, "\033[36m");

    Packet request(0, std::vector<char>(data, data + len), ChromaFlag::GET);
    if (sendPacket(request, serverAddr) < 0) {
        throw std::runtime_error("Falha ao enviar requisição para o servidor");
    }

    int retries = 3;
    Packet pkt;
    while (retries-- > 0) {
        if (waitResponse(5) && recvPacket(pkt) > 0) {
            if (pkt.flag == ChromaFlag::META && !isCorrupted(pkt)) {
                serverResponseAddr = pkt.srcAddr;
                readFileMetadata(pkt);

                Packet ackMeta(0, {}, ChromaFlag::ACK);
                sendPacket(ackMeta, serverResponseAddr);
                logMsg("Contato estabelecido com a thread do servidor.", "\033[32m");
                bufferPackets.clear();
                base = 0;
                nextSeqNum = 0;
                receiveData();
                return;
            }
        }
        logErr("Falha ao estabelecer contato inicial, tentando novamente...");
        sendPacket(request, serverAddr);
    }
    logErr("Falha ao tentar se comunicar com o servidor após múltiplas tentativas.");
}

void ChromaClient::receiveData() {
    logMsg("Aguardando pacotes do servidor...", "\033[36m");

    bool transmissionEnded = false;
    std::ofstream file(("arquivo_reconstruido_" + filename + "." + extensionFile).c_str(),
                       std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Erro ao criar arquivo de saída");
    }

    while (!transmissionEnded) {
        Packet pkt;

        if (!waitResponse(10) || recvPacket(pkt) <= 0) {
            logErr("Timeout ou erro de recepção.");
            break;
        }

        if (isCorrupted(pkt)) {
            logErr("Pacote corrompido descartado.", "\033[33m");
            continue;
        }

        switch (pkt.flag) {
            case ChromaFlag::DATA: {
                if (isSeqInWindow(pkt.seqNum, base)) {
                    if (bufferPackets.find(pkt.seqNum) != bufferPackets.end()) {
                        logMsg("Pacote duplicado Seq=" + std::to_string(pkt.seqNum) + " → reenviando ACK.", "\033[35m");
                        sendConfirmation(pkt.seqNum, ChromaFlag::ACK, serverResponseAddr);
                        break;
                    }

                    bufferPackets.insert({pkt.seqNum, pkt});
                    logMsg("Pacote Seq=" + std::to_string(pkt.seqNum) +
                           " (" + std::to_string(pkt.data.size()) + " bytes) recebido.", "\033[34m");

                    while (bufferPackets.find(base) != bufferPackets.end()) {
                        Packet& inOrder = bufferPackets[base];
                        file.write(inOrder.data.data(), inOrder.data.size());
                        bufferPackets.erase(base);
                        base++;
                    }

                    sendConfirmation(pkt.seqNum, ChromaFlag::ACK, serverResponseAddr);
                }
                break;
            }

            case ChromaFlag::END:
                logMsg("Fim de transmissão.", "\033[32m");
                transmissionEnded = true;
                break;

            case ChromaFlag::NACK:
                logErr("Servidor não encontrou o arquivo.");
                transmissionEnded = true;
                break;

            default:
                logErr("Flag desconhecida ignorada.", "\033[33m");
                break;
        }
    }

    file.flush();
    file.close();
    logMsg("Arquivo salvo com sucesso!", "\033[32m");
}

void ChromaClient::readFileMetadata(const Packet& pkt) {
    std::string dataStr(pkt.data.begin(), pkt.data.end());
    std::istringstream iss(dataStr);
    std::string fname, ext, sizeStr, totalStr;

    if (!std::getline(iss, fname, '\0') ||
        !std::getline(iss, ext, '\0') ||
        !std::getline(iss, sizeStr, '\0') ||
        !std::getline(iss, totalStr, '\0')) {
        throw std::runtime_error("Erro ao interpretar metadados.");
    }

    filename = fname.substr(0, fname.find_last_of('.'));
    extensionFile = ext;
    fileSize = std::stoll(sizeStr);
    totalPackets = std::stoi(totalStr);

    logMsg("Metadados recebidos:", "\033[32m");
    logMsg("Arquivo: " + filename);
    logMsg("Extensão: " + extensionFile);
    logMsg("Tamanho: " + std::to_string(fileSize) + " bytes");
    logMsg("Pacotes esperados: " + std::to_string(totalPackets));
}