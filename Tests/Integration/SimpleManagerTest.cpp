#include <gtest/gtest.h>

#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Network;

class SimpleManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Just test basic functionality
    }

    void TearDown() override
    {
        // Nothing to clean up
    }
};

// Test basic network types and operations
TEST_F(SimpleManagerTest, BasicNetworkOperations)
{
    // Test NetworkAddress operations
    NetworkAddress TestAddress("127.0.0.1", 8080);
    EXPECT_EQ(TestAddress.Ip, "127.0.0.1");
    EXPECT_EQ(TestAddress.Port, 8080);
    EXPECT_TRUE(TestAddress.IsValid());
    
    std::string AddressString = TestAddress.ToString();
    EXPECT_EQ(AddressString, "127.0.0.1:8080");
    
    // Test ConnectionState operations
    ConnectionState State = ConnectionState::DISCONNECTED;
    EXPECT_EQ(State, ConnectionState::DISCONNECTED);
    
    State = ConnectionState::CONNECTING;
    EXPECT_EQ(State, ConnectionState::CONNECTING);
    
    State = ConnectionState::CONNECTED;
    EXPECT_EQ(State, ConnectionState::CONNECTED);
    
    // Test ProtocolType operations
    ProtocolType Protocol = ProtocolType::TCP;
    EXPECT_EQ(Protocol, ProtocolType::TCP);
    
    Protocol = ProtocolType::UDP;
    EXPECT_EQ(Protocol, ProtocolType::UDP);
    
    // Test NetworkError operations
    NetworkError Error = NetworkError::SUCCESS;
    EXPECT_EQ(Error, NetworkError::SUCCESS);
    
    Error = NetworkError::CONNECTION_FAILED;
    EXPECT_EQ(Error, NetworkError::CONNECTION_FAILED);
}

// Test NetworkConfig
TEST_F(SimpleManagerTest, NetworkConfig)
{
    NetworkConfig Config;
    EXPECT_EQ(Config.MaxConnections, HELIANTHUS_MAX_CONNECTIONS);
    EXPECT_EQ(Config.BufferSizeBytes, HELIANTHUS_DEFAULT_BUFFER_SIZE);
    EXPECT_FALSE(Config.NoDelay);
    EXPECT_FALSE(Config.ReuseAddr);
    EXPECT_FALSE(Config.KeepAlive);
    EXPECT_EQ(Config.ConnectionTimeoutMs, HELIANTHUS_NETWORK_TIMEOUT_MS);
    EXPECT_EQ(Config.KeepAliveIntervalMs, 30000);
    EXPECT_EQ(Config.KeepAliveIdleSec, 60);
    EXPECT_EQ(Config.KeepAliveProbes, 5);
    EXPECT_EQ(Config.ThreadPoolSize, HELIANTHUS_DEFAULT_THREAD_POOL_SIZE);
    EXPECT_FALSE(Config.EnableNagle);
    EXPECT_TRUE(Config.EnableKeepalive);
    EXPECT_FALSE(Config.EnableCompression);
    EXPECT_FALSE(Config.EnableEncryption);
}
