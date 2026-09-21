#pragma once

#include "../Shared/PacketProtocol.h"

#include <winsock2.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

enum class PacketReadResult
{
    NeedMoreData,
    PacketReady,
    InvalidPacket
};

class ClientSession
{
public:
    ClientSession(SOCKET socket, std::uint32_t id);
    ~ClientSession();

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;

    SOCKET GetSocket() const;
    std::uint32_t GetId() const;
    bool HasPendingSend() const;

    void Queue(const void* data, std::size_t size);
    void Queue(const std::vector<char>& packet);
    bool FlushSendQueue();
    bool Receive();
    PacketReadResult TryReadPacket(std::vector<char>& packet);
    void Close();

private:
    struct PendingSend
    {
        std::vector<char> bytes;
        std::size_t offset = 0;
    };

    static constexpr std::size_t kMaximumPacketSize = 1024;
    static constexpr std::size_t kMaximumBufferedBytes = kMaximumPacketSize * 4;

    static std::size_t GetExpectedPacketSize(PacketType type);

    SOCKET m_socket = INVALID_SOCKET;
    std::uint32_t m_id = 0;
    std::vector<char> m_receiveBuffer;
    std::deque<PendingSend> m_sendQueue;
};
