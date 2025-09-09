#include <gtest/gtest.h>

#include "Shared/Network/ConnectionManager.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Network;

class SimpleConnectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize connection manager
        NetworkConfig NetworkConfig;
        NetworkConfig.MaxConnections = 100;
        NetworkConfig.ConnectionTimeoutMs = 5000;
        NetworkConfig.EnableKeepalive = true;
        
        NetworkError Result = ConnectionManagerInstance.Initialize(NetworkConfig);
        ASSERT_EQ(Result, NetworkError::SUCCESS);
    }

    void TearDown() override
    {
        ConnectionManagerInstance.Shutdown();
    }

    ConnectionManager ConnectionManagerInstance;
};

// Test basic connection creation
TEST_F(SimpleConnectionTest, BasicConnectionCreation)
{
    // Create a connection
    NetworkAddress TestAddress("127.0.0.1", 8080);
    ConnectionId ConnectionId = ConnectionManagerInstance.CreateConnection(TestAddress, ProtocolType::TCP);
    
    EXPECT_NE(ConnectionId, InvalidConnectionId);
    EXPECT_EQ(ConnectionManagerInstance.GetConnectionCount(), 1);
    
    // Test connection info
    ConnectionInfo* Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_EQ(Info->Address.Ip, "127.0.0.1");
    EXPECT_EQ(Info->Address.Port, 8080);
    EXPECT_EQ(Info->Protocol, ProtocolType::TCP);
    EXPECT_EQ(Info->State, ConnectionState::DISCONNECTED);
}

// Test connection without connecting
TEST_F(SimpleConnectionTest, ConnectionWithoutConnect)
{
    NetworkAddress TestAddress("127.0.0.1", 8080);
    ConnectionId ConnectionId = ConnectionManagerInstance.CreateConnection(TestAddress, ProtocolType::TCP);
    
    EXPECT_NE(ConnectionId, InvalidConnectionId);
    
    // Don't call Connect() - just verify the connection exists
    ConnectionInfo* Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_EQ(Info->State, ConnectionState::DISCONNECTED);
    
    // Test disconnect
    ConnectionManagerInstance.Disconnect(ConnectionId);
    Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_EQ(Info->State, ConnectionState::DISCONNECTED);
}
