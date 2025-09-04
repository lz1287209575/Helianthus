#include "Shared/Network/Security/ITlsChannel.h"

#include "Shared/Network/Security/MockTlsChannel.h"

namespace Helianthus::Network::Security
{
std::unique_ptr<ITlsChannel> CreateTlsChannel()
{
    return std::make_unique<MockTlsChannel>();
}
}  // namespace Helianthus::Network::Security
