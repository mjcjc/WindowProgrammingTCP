#pragma once

#include <cstdint>

enum PacketType : std::int32_t
{
    GET_PLAYER_PACKET,
    MESH_PACKET,
    KEY_CONTROL_PACKET,
    ITEM_PACKET,
    BOARD_STATE_PACKET
};

constexpr std::int32_t MAX_SYNCHRONIZED_OBJECTS = 32;

#pragma pack(push, 1)

struct PlayerPacket
{
    PacketType packetType;
    std::int32_t size;
};

struct getPlayer
{
    PacketType packetType = GET_PLAYER_PACKET;
    std::int32_t count = 0;
};

struct PlayerMeshPacket
{
    PacketType packetType = MESH_PACKET;
    std::int32_t size = 0;
    float m_fxPosition = 0.0f;
    float m_fyPosition = 0.0f;
    std::int32_t meshIndex = 0;
    std::int32_t topPositionRate = 0;
    bool Win = false;
};

struct PlayerKeyControlPacket
{
    PacketType packetType = KEY_CONTROL_PACKET;
    std::int32_t size = 0;
    std::uint32_t nMessageID = 0;
};

struct PlayerItemPacket
{
    PacketType packetType = ITEM_PACKET;
    std::int32_t m_nChangeBlockCount = 0;
};

struct BoardObjectState
{
    std::int32_t meshIndex = 0;
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
};

struct BoardStatePacket
{
    PacketType packetType = BOARD_STATE_PACKET;
    std::int32_t size = sizeof(BoardStatePacket);
    std::uint32_t sequence = 0;
    std::int32_t objectCount = 0;
    std::int32_t nextMeshIndex = 0;
    std::int32_t senderPlayerNumber = 0;
    float remainingTime = 100.0f;
    float cameraY = 0.0f;
    float nextObjectX = 0.0f;
    float nextObjectY = 0.0f;
    BoardObjectState objects[MAX_SYNCHRONIZED_OBJECTS]{};
};

#pragma pack(pop)

static_assert(sizeof(PlayerPacket) == 8);
static_assert(sizeof(PlayerMeshPacket) == 25);
static_assert(sizeof(PlayerKeyControlPacket) == 12);
static_assert(sizeof(BoardObjectState) == 16);
static_assert(sizeof(BoardStatePacket) == 552);
