#include "GameServer.h"

#include "ClientSession.h"

#include <ws2tcpip.h>

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")

GameServer::GameServer() = default;

GameServer::~GameServer()
{
    Shutdown();
}

bool GameServer::Initialize(unsigned short port)
{
    if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0)
        return false;
    m_wsaInitialized = true;

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET)
    {
        PrintSocketError("socket");
        return false;
    }

    BOOL reuseAddress = TRUE;
    setsockopt(
        m_listenSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuseAddress),
        sizeof(reuseAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        PrintSocketError("bind");
        return false;
    }
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        PrintSocketError("listen");
        return false;
    }
    if (!SetNonBlocking(m_listenSocket))
    {
        PrintSocketError("ioctlsocket");
        return false;
    }

    std::printf("Inversus server listening on TCP port %u\n", port);
    return true;
}

int GameServer::Run()
{
    while (m_listenSocket != INVALID_SOCKET)
    {
        fd_set readSet;
        fd_set writeSet;
        BuildSocketSets(readSet, writeSet);

        if (select(0, &readSet, &writeSet, nullptr, nullptr) == SOCKET_ERROR)
        {
            PrintSocketError("select");
            return 1;
        }

        if (FD_ISSET(m_listenSocket, &readSet))
            AcceptPendingClients();

        MatchPlayersIfReady();
        if (!ProcessClients(readSet, writeSet))
            ResetRoom();
    }
    return 0;
}

void GameServer::Shutdown()
{
    ResetRoom();
    if (m_listenSocket != INVALID_SOCKET)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    if (m_wsaInitialized)
    {
        WSACleanup();
        m_wsaInitialized = false;
    }
}

void GameServer::PrintSocketError(const char* operation)
{
    std::printf("[error] %s failed: %d\n", operation, WSAGetLastError());
}

bool GameServer::SetNonBlocking(SOCKET socket)
{
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) != SOCKET_ERROR;
}

void GameServer::BuildSocketSets(fd_set& readSet, fd_set& writeSet)
{
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_SET(m_listenSocket, &readSet);
    for (const std::unique_ptr<ClientSession>& client : m_clients)
    {
        FD_SET(client->GetSocket(), &readSet);
        if (client->HasPendingSend())
            FD_SET(client->GetSocket(), &writeSet);
    }
}

void GameServer::AcceptPendingClients()
{
    while (true)
    {
        sockaddr_in clientAddress{};
        int addressLength = sizeof(clientAddress);
        SOCKET clientSocket = accept(
            m_listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &addressLength);

        if (clientSocket == INVALID_SOCKET)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                PrintSocketError("accept");
            return;
        }

        if (m_clients.size() >= kPlayersPerRoom || !SetNonBlocking(clientSocket))
        {
            closesocket(clientSocket);
            continue;
        }

        auto client = std::make_unique<ClientSession>(clientSocket, m_nextClientId++);
        std::printf("player %u connected\n", client->GetId());
        m_clients.push_back(std::move(client));
    }
}

void GameServer::MatchPlayersIfReady()
{
    if (m_gameStarted || m_clients.size() != kPlayersPerRoom)
        return;

    const bool startSignal = true;
    for (const std::unique_ptr<ClientSession>& client : m_clients)
        client->Queue(&startSignal, sizeof(startSignal));

    m_gameStarted = true;
    m_gameStartTime = std::chrono::steady_clock::now();
    std::printf("two players matched; game start queued\n");
}

bool GameServer::ProcessClients(fd_set& readSet, fd_set& writeSet)
{
    for (std::size_t index = 0; index < m_clients.size(); ++index)
    {
        ClientSession& client = *m_clients[index];
        if (FD_ISSET(client.GetSocket(), &writeSet) && !client.FlushSendQueue())
            return false;
        if (!FD_ISSET(client.GetSocket(), &readSet))
            continue;
        if (!client.Receive())
            return false;
        if (!m_gameStarted)
            continue;

        ClientSession& peer = *m_clients[index == 0 ? 1 : 0];
        while (true)
        {
            std::vector<char> packet;
            const PacketReadResult result = client.TryReadPacket(packet);
            if (result == PacketReadResult::NeedMoreData)
                break;
            if (result == PacketReadResult::InvalidPacket)
                return false;

            if (packet.size() == sizeof(BoardStatePacket))
            {
                BoardStatePacket boardState{};
                std::memcpy(&boardState, packet.data(), sizeof(boardState));
                if (boardState.packetType == BOARD_STATE_PACKET)
                {
                    const float elapsedSeconds = std::chrono::duration<float>(
                        std::chrono::steady_clock::now() - m_gameStartTime).count();
                    boardState.senderPlayerNumber = static_cast<std::int32_t>(index + 1);
                    boardState.remainingTime = (std::max)(0.0f, 100.0f - elapsedSeconds);
                    std::memcpy(packet.data(), &boardState, sizeof(boardState));
                }
            }
            peer.Queue(packet);
        }
    }
    return true;
}

void GameServer::ResetRoom()
{
    if (!m_clients.empty())
        std::printf("session closed; waiting for two new players\n");
    m_clients.clear();
    m_gameStarted = false;
}
