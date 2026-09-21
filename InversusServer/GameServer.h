#pragma once

#include <winsock2.h>

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <memory>
#include <vector>

class ClientSession;

class GameServer
{
public:
    GameServer();
    ~GameServer();

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;

    bool Initialize(unsigned short port);
    int Run();
    void Shutdown();

private:
    static constexpr std::size_t kPlayersPerRoom = 2;

    static void PrintSocketError(const char* operation);
    static bool SetNonBlocking(SOCKET socket);

    void BuildSocketSets(fd_set& readSet, fd_set& writeSet);
    void AcceptPendingClients();
    void MatchPlayersIfReady();
    bool ProcessClients(fd_set& readSet, fd_set& writeSet);
    void ResetRoom();

    WSADATA m_wsaData{};
    bool m_wsaInitialized = false;
    SOCKET m_listenSocket = INVALID_SOCKET;
    std::uint32_t m_nextClientId = 1;
    bool m_gameStarted = false;
    std::chrono::steady_clock::time_point m_gameStartTime{};
    std::vector<std::unique_ptr<ClientSession>> m_clients;
};
