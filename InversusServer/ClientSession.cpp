#include "ClientSession.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <utility>

ClientSession::ClientSession(SOCKET socket, std::uint32_t id)
    : m_socket(socket), m_id(id)
{
}

ClientSession::~ClientSession()
{
    Close();
}

SOCKET ClientSession::GetSocket() const
{
    return m_socket;
}

std::uint32_t ClientSession::GetId() const
{
    return m_id;
}

bool ClientSession::HasPendingSend() const
{
    return !m_sendQueue.empty();
}

void ClientSession::Queue(const void* data, std::size_t size)
{
    PendingSend pending;
    const char* first = static_cast<const char*>(data);
    pending.bytes.assign(first, first + size);
    m_sendQueue.push_back(std::move(pending));
}

void ClientSession::Queue(const std::vector<char>& packet)
{
    Queue(packet.data(), packet.size());
}

bool ClientSession::FlushSendQueue()
{
    while (!m_sendQueue.empty())
    {
        PendingSend& pending = m_sendQueue.front();
        const std::size_t remaining = pending.bytes.size() - pending.offset;
        const int sent = send(
            m_socket,
            pending.bytes.data() + pending.offset,
            static_cast<int>(remaining),
            0);

        if (sent == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                return true;
            return false;
        }
        if (sent == 0)
            return false;

        pending.offset += static_cast<std::size_t>(sent);
        if (pending.offset == pending.bytes.size())
            m_sendQueue.pop_front();
    }
    return true;
}

bool ClientSession::Receive()
{
    std::array<char, 4096> temporaryBuffer{};
    const int received = recv(
        m_socket,
        temporaryBuffer.data(),
        static_cast<int>(temporaryBuffer.size()),
        0);

    if (received == SOCKET_ERROR)
        return WSAGetLastError() == WSAEWOULDBLOCK;
    if (received == 0)
        return false;

    m_receiveBuffer.insert(
        m_receiveBuffer.end(),
        temporaryBuffer.data(),
        temporaryBuffer.data() + received);
    return m_receiveBuffer.size() <= kMaximumBufferedBytes;
}

PacketReadResult ClientSession::TryReadPacket(std::vector<char>& packet)
{
    if (m_receiveBuffer.size() < sizeof(PlayerPacket))
        return PacketReadResult::NeedMoreData;

    PlayerPacket header{};
    std::memcpy(&header, m_receiveBuffer.data(), sizeof(header));
    const std::size_t expectedSize = GetExpectedPacketSize(header.packetType);

    if (expectedSize == 0 ||
        header.size != static_cast<std::int32_t>(expectedSize) ||
        expectedSize > kMaximumPacketSize)
    {
        std::printf(
            "[error] invalid packet from player %u: type=%d size=%d\n",
            m_id,
            static_cast<int>(header.packetType),
            header.size);
        return PacketReadResult::InvalidPacket;
    }

    if (m_receiveBuffer.size() < expectedSize)
        return PacketReadResult::NeedMoreData;

    packet.assign(
        m_receiveBuffer.begin(),
        m_receiveBuffer.begin() + static_cast<std::ptrdiff_t>(expectedSize));
    m_receiveBuffer.erase(
        m_receiveBuffer.begin(),
        m_receiveBuffer.begin() + static_cast<std::ptrdiff_t>(expectedSize));
    return PacketReadResult::PacketReady;
}

void ClientSession::Close()
{
    if (m_socket == INVALID_SOCKET)
        return;
    shutdown(m_socket, SD_BOTH);
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
}

std::size_t ClientSession::GetExpectedPacketSize(PacketType type)
{
    switch (type)
    {
    case MESH_PACKET:
        return sizeof(PlayerMeshPacket);
    case KEY_CONTROL_PACKET:
        return sizeof(PlayerKeyControlPacket);
    case BOARD_STATE_PACKET:
        return sizeof(BoardStatePacket);
    default:
        return 0;
    }
}
