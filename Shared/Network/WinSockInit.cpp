#include "Shared/Network/WinSockInit.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
#endif

namespace Helianthus::Network
{
void EnsureWinSockInitialized()
{
#ifdef _WIN32
    static bool Initialized = false;
    if (!Initialized)
    {
        WSADATA WsaData{};
        if (WSAStartup(MAKEWORD(2, 2), &WsaData) == 0)
        {
            Initialized = true;
        }
    }
#endif
}
}  // namespace Helianthus::Network
