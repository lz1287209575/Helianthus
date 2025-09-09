#include <gtest/gtest.h>

#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Network;

class MinimalTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Just test basic types
    }

    void TearDown() override
    {
        // Nothing to clean up
    }
};

// Test basic network types
TEST_F(MinimalTest, BasicNetworkTypes)
{
    // Test NetworkAddress
    NetworkAddress TestAddress("127.0.0.1", 8080);
    EXPECT_EQ(TestAddress.Ip, "127.0.0.1");
    EXPECT_EQ(TestAddress.Port, 8080);
    EXPECT_TRUE(TestAddress.IsValid());
    
    // Test ConnectionState
    ConnectionState State = ConnectionState::DISCONNECTED;
    EXPECT_EQ(State, ConnectionState::DISCONNECTED);
    
    // Test ProtocolType
    ProtocolType Protocol = ProtocolType::TCP;
    EXPECT_EQ(Protocol, ProtocolType::TCP);
    
    // Test NetworkError
    NetworkError Error = NetworkError::SUCCESS;
    EXPECT_EQ(Error, NetworkError::SUCCESS);
}

// Test ConnectionStats
TEST_F(MinimalTest, ConnectionStats)
{
    ConnectionStats Stats;
    EXPECT_EQ(Stats.BytesSent, 0);
    EXPECT_EQ(Stats.BytesReceived, 0);
    EXPECT_EQ(Stats.PacketsSent, 0);
    EXPECT_EQ(Stats.PacketsReceived, 0);
    EXPECT_EQ(Stats.ConnectionTimeMs, 0);
    EXPECT_EQ(Stats.PingMs, 0);
}
