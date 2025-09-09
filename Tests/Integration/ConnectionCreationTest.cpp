#include <gtest/gtest.h>

#include "Shared/Network/ConnectionManager.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Network;

class ConnectionCreationTest : public ::testing::Test
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

// Test connection creation only
TEST_F(ConnectionCreationTest, CreateConnectionOnly)
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

// Test multiple connection creation
TEST_F(ConnectionCreationTest, CreateMultipleConnections)
{
    const int NumConnections = 3;
    std::vector<ConnectionId> ConnectionIds;
    
    // Create multiple connections
    for (int i = 0; i < NumConnections; ++i)
    {
        NetworkAddress Address("127.0.0.1", 8080 + i);
        ConnectionId Id = ConnectionManagerInstance.CreateConnection(Address, ProtocolType::TCP);
        EXPECT_NE(Id, InvalidConnectionId);
        ConnectionIds.push_back(Id);
    }
    
    EXPECT_EQ(ConnectionManagerInstance.GetConnectionCount(), NumConnections);
    
    // Verify all connections exist
    for (ConnectionId Id : ConnectionIds)
    {
        ConnectionInfo* Info = ConnectionManagerInstance.GetConnection(Id);
        ASSERT_NE(Info, nullptr);
        EXPECT_EQ(Info->State, ConnectionState::DISCONNECTED);
    }
}
