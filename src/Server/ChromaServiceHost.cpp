#include "ChromaServiceHost.hpp"
#include "ChromaServer.hpp"

ChromaServiceHost::ChromaServiceHost(int winSize, int bufSize, int port) : ChromaProtocol(winSize, bufSize), running(false), serverPort(port), limitConnections(5)
{
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(serverPort);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("Erro ao bindar socket do gerenciador de requisições");
    }

    socklen_t len = sizeof(addr);

    if (getsockname(sockfd, (struct sockaddr*)&addr, &len) < 0) {
        throw std::runtime_error("Erro ao obter porta atribuída ao gerenciador de requisições");
    }

    std::cout << "ChromaServiceHost rodando na porta: " << ntohs(addr.sin_port) << std::endl << "Com ip: " << inet_ntoa(addr.sin_addr) << std::endl;
}

ChromaServiceHost::~ChromaServiceHost() {
    StopServer();
}
    
void ChromaServiceHost::start() {
    running = true;
    while (running)
    {
        Packet pkt;

        std::cout << "Aguardando requisição de cliente..." << std::endl;
        
        if (recvPacket(pkt) < 0) {
            std::cerr << "Erro ao receber pacote" << std::endl;
            continue;
        }

        if (isCorrupted(pkt)) {
            std::cerr << "Pacote corrompido recebido" << std::endl;
            continue;
        }

        std::cout << "Pacote recebido do cliente: " << inet_ntoa(pkt.srcAddr.sin_addr) << ":" << ntohs(pkt.srcAddr.sin_port) << std::endl;

        CreateServer(inet_ntoa(pkt.srcAddr.sin_addr), pkt);
    }
}

void ChromaServiceHost::CreateServer(const char* ip, Packet pkt) {
    std::thread([this, pkt]()
    {
        try 
        {
            ChromaServer server(windowSize, bufferSize, pkt.srcAddr);

            server.sendData(std::string(pkt.data.begin(), pkt.data.end()).c_str(), 1000);

        } catch (const std::exception& e)
        {
            std::cerr << "Erro ao iniciar servidor para cliente: " << e.what() << std::endl;
        }
    }).detach();
}

void ChromaServiceHost::StopServer()
{
    if (running) {
        running = false;
        close(sockfd);
        std::cout << "ChromaServiceHost parado." << std::endl;
    }
}