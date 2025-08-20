#pragma once

namespace Helianthus::Network
{
// Ensure WinSock is initialized on Windows. No-op on other platforms.
void EnsureWinSockInitialized();
}  // namespace Helianthus::Network
