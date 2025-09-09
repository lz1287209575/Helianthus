#include <gtest/gtest.h>

#include "Shared/RPC/IRpcServer.h"
#include "Shared/RPC/IRpcClient.h"
#include "Shared/RPC/RpcTypes.h"
#include "Shared/Network/ConnectionManager.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::RPC;
using namespace Helianthus::Network;

class RpcNetworkIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize connection manager
        NetworkConfig NetworkConfig;
        NetworkConfig.MaxConnections = 100;
        NetworkConfig.ConnectionTimeoutMs = 5000;
        NetworkConfig.EnableKeepalive = true;
        
        ConnectionManagerResult = ConnectionManagerInstance.Initialize(NetworkConfig);
        ASSERT_EQ(ConnectionManagerResult, NetworkError::SUCCESS);
    }

    void TearDown() override
    {
        ConnectionManagerInstance.Shutdown();
    }

    ConnectionManager ConnectionManagerInstance;
    NetworkError ConnectionManagerResult;
};

// Test basic connection management
TEST_F(RpcNetworkIntegrationTest, BasicConnectionManagement)
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
    
    // Test connection
    NetworkError ConnectResult = ConnectionManagerInstance.Connect(ConnectionId);
    EXPECT_EQ(ConnectResult, NetworkError::SUCCESS);
    
    // Verify connection state
    Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_EQ(Info->State, ConnectionState::CONNECTED);
    
    // Test active connections
    auto ActiveConnections = ConnectionManagerInstance.GetActiveConnections();
    EXPECT_EQ(ActiveConnections.size(), 1);
    EXPECT_EQ(ActiveConnections[0], ConnectionId);
    
    // Test disconnect
    ConnectionManagerInstance.Disconnect(ConnectionId);
    Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_EQ(Info->State, ConnectionState::DISCONNECTED);
}

// Test multiple connections
TEST_F(RpcNetworkIntegrationTest, MultipleConnections)
{
    const int NumConnections = 5;
    std::vector<uint64_t> ConnectionIds;
    
    // Create multiple connections
    for (int i = 0; i < NumConnections; ++i)
    {
        NetworkAddress Address("127.0.0.1", 8080 + i);
        auto Id = ConnectionManagerInstance.CreateConnection(Address, ProtocolType::TCP);
        EXPECT_NE(Id, InvalidConnectionId);
        ConnectionIds.push_back(Id);
    }
    
    EXPECT_EQ(ConnectionManagerInstance.GetConnectionCount(), NumConnections);
    
    // Connect all
    for (ConnectionId Id : ConnectionIds)
    {
        NetworkError Result = ConnectionManagerInstance.Connect(Id);
        EXPECT_EQ(Result, NetworkError::SUCCESS);
    }
    
    // Verify all are connected
    auto ActiveConnections = ConnectionManagerInstance.GetActiveConnections();
    EXPECT_EQ(ActiveConnections.size(), NumConnections);
    
    // Test broadcast
    const char* TestData = "Hello World";
    NetworkError BroadcastResult = ConnectionManagerInstance.BroadcastData(TestData, 11);
    EXPECT_EQ(BroadcastResult, NetworkError::SUCCESS);
    
    // Test statistics
    ConnectionStats TotalStats = ConnectionManagerInstance.GetTotalStats();
    EXPECT_GT(TotalStats.BytesSent, 0);
}

// Test reconnection settings
TEST_F(RpcNetworkIntegrationTest, ReconnectionSettings)
{
    NetworkAddress TestAddress("127.0.0.1", 8080);
    ConnectionId ConnectionId = ConnectionManagerInstance.CreateConnection(TestAddress, ProtocolType::TCP);
    
    // Test auto-reconnect settings
    ConnectionManagerInstance.EnableAutoReconnect(ConnectionId, true);
    ConnectionManagerInstance.SetReconnectionSettings(ConnectionId, 5, 2000);
    
    ConnectionInfo* Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_EQ(Info->MaxRetries, 5);
    EXPECT_EQ(Info->RetryIntervalMs, 2000);
    
    // Test heartbeat settings
    ConnectionManagerInstance.EnableHeartbeat(ConnectionId, true);
    ConnectionManagerInstance.SetHeartbeatSettings(ConnectionId, 10000, 3);
    
    Info = ConnectionManagerInstance.GetConnection(ConnectionId);
    ASSERT_NE(Info, nullptr);
    EXPECT_TRUE(Info->EnableHeartbeat);
    EXPECT_EQ(Info->HeartbeatIntervalMs, 10000);
    EXPECT_EQ(Info->MaxMissedHeartbeats, 3);
}

// Test connection state changes
TEST_F(RpcNetworkIntegrationTest, ConnectionStateChanges)
{
    bool StateChangedCalled = false;
    ConnectionId ChangedConnectionId = InvalidConnectionId;
    ConnectionState OldState = ConnectionState::DISCONNECTED;
    ConnectionState NewState = ConnectionState::DISCONNECTED;
    
    // Set up state change handler
    ConnectionManagerInstance.SetOnConnectionStateChanged(
        [&](ConnectionId Id, ConnectionState Old, ConnectionState New)
        {
            StateChangedCalled = true;
            ChangedConnectionId = Id;
            OldState = Old;
            NewState = New;
        }
    );
    
    NetworkAddress TestAddress("127.0.0.1", 8080);
    ConnectionId ConnectionId = ConnectionManagerInstance.CreateConnection(TestAddress, ProtocolType::TCP);
    
    // Connect and verify state change callback
    NetworkError ConnectResult = ConnectionManagerInstance.Connect(ConnectionId);
    EXPECT_EQ(ConnectResult, NetworkError::SUCCESS);
    
    // Process events to trigger callbacks
    ConnectionManagerInstance.ProcessEvents();
    
    EXPECT_TRUE(StateChangedCalled);
    EXPECT_EQ(ChangedConnectionId, ConnectionId);
    EXPECT_EQ(OldState, ConnectionState::CONNECTING);
    EXPECT_EQ(NewState, ConnectionState::CONNECTED);
}

// Test data sending
TEST_F(RpcNetworkIntegrationTest, DataSending)
{
    NetworkAddress TestAddress("127.0.0.1", 8080);
    ConnectionId ConnectionId = ConnectionManagerInstance.CreateConnection(TestAddress, ProtocolType::TCP);
    
    // Connect first
    NetworkError ConnectResult = ConnectionManagerInstance.Connect(ConnectionId);
    EXPECT_EQ(ConnectResult, NetworkError::SUCCESS);
    
    // Test sending data
    const char* TestData = "Test Message";
    size_t DataSize = 12;
    
    NetworkError SendResult = ConnectionManagerInstance.SendData(ConnectionId, TestData, DataSize);
    EXPECT_EQ(SendResult, NetworkError::SUCCESS);
    
    // Verify statistics
    ConnectionStats Stats = ConnectionManagerInstance.GetConnectionStats(ConnectionId);
    EXPECT_EQ(Stats.BytesSent, DataSize);
    
    // Test sending to multiple connections
    std::vector<uint64_t> ConnectionIds;
    for (int i = 0; i < 3; ++i)
    {
        NetworkAddress Address("127.0.0.1", 8081 + i);
        auto Id = ConnectionManagerInstance.CreateConnection(Address, ProtocolType::TCP);
        ConnectionManagerInstance.Connect(Id);
        ConnectionIds.push_back(Id);
    }
    
    NetworkError MultiSendResult = ConnectionManagerInstance.SendToConnections(ConnectionIds, TestData, DataSize);
    EXPECT_EQ(MultiSendResult, NetworkError::SUCCESS);
    
    // Verify total statistics
    ConnectionStats TotalStats = ConnectionManagerInstance.GetTotalStats();
    EXPECT_GT(TotalStats.BytesSent, DataSize);
}

// Test cleanup
TEST_F(RpcNetworkIntegrationTest, Cleanup)
{
    // Create and connect multiple connections
    std::vector<uint64_t> ConnectionIds;
    for (int i = 0; i < 3; ++i)
    {
        NetworkAddress Address("127.0.0.1", 8080 + i);
        auto Id = ConnectionManagerInstance.CreateConnection(Address, ProtocolType::TCP);
        ConnectionManagerInstance.Connect(Id);
        ConnectionIds.push_back(Id);
    }
    
    EXPECT_EQ(ConnectionManagerInstance.GetConnectionCount(), 3);
    
    // Disconnect all
    ConnectionManagerInstance.DisconnectAll();
    
    // Verify all are disconnected
    auto ActiveConnections = ConnectionManagerInstance.GetActiveConnections();
    EXPECT_EQ(ActiveConnections.size(), 0);
    
    // Verify connection count is still the same (connections exist but are disconnected)
    EXPECT_EQ(ConnectionManagerInstance.GetConnectionCount(), 3);
}