#include "GameServer.h"

int main()
{
    GameServer server;
    if (!server.Initialize(9000))
        return 1;
    return server.Run();
}
