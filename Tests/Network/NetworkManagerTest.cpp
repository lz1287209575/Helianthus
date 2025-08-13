#include <gtest/gtest.h>
#include "Network/NetworkManager.h"
#include "Network/NetworkTypes.h"
#include <thread>
#include <chrono>

using namespace Helianthus::Network;

class NetworkManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Config_.MaxConnections = 100;
        Config_.ConnectionTimeoutMs = 5000;
        Config_.KeepAliveIntervalMs = 1000;
        Config_.BufferSizeBytes = 8192;
        Config_.EnableCompression = false;
        Config_.EnableEncryption = false;
        
        Manager_ = std::make_unique<NetworkManager>();
    }

    void TearDown() override
    {
        if (Manager_)
        {
            Manager_->Shutdown();
        }
    }

    NetworkAddress CreateTestAddress(const std::string& Host = "127.0.0.1", uint16_t Port = 8080)
    {
        NetworkAddress Address;
        Address.Host = Host;
        Address.Port = Port;
        Address.Protocol = ProtocolType::TCP;
        return Address;
    }

    NetworkConfig Config_;
    std::unique_ptr<NetworkManager> Manager_;
};

TEST_F(NetworkManagerTest, InitializationWorksCorrectly)
{
    EXPECT_FALSE(Manager_->IsInitialized());
    
    auto Result = Manager_->Initialize(Config_);
    EXPECT_EQ(Result, NetworkError::SUCCESS);
    EXPECT_TRUE(Manager_->IsInitialized());
    
    // Double initialization should fail
    Result = Manager_->Initialize(Config_);
    EXPECT_EQ(Result, NetworkError::ALREADY_INITIALIZED);
}

TEST_F(NetworkManagerTest, ConfigurationManagement)
{
    Manager_->Initialize(Config_);
    
    auto CurrentConfig = Manager_->GetCurrentConfig();
    EXPECT_EQ(CurrentConfig.MaxConnections, Config_.MaxConnections);
    EXPECT_EQ(CurrentConfig.ConnectionTimeoutMs, Config_.ConnectionTimeoutMs);
    
    // Update configuration
    NetworkConfig NewConfig = Config_;
    NewConfig.MaxConnections = 200;
    NewConfig.BufferSizeBytes = 16384;
    
    Manager_->UpdateConfig(NewConfig);
    
    auto UpdatedConfig = Manager_->GetCurrentConfig();
    EXPECT_EQ(UpdatedConfig.MaxConnections, 200);
    EXPECT_EQ(UpdatedConfig.BufferSizeBytes, 16384);
}

TEST_F(NetworkManagerTest, ConnectionManagement)
{
    Manager_->Initialize(Config_);
    
    // Initially no connections
    auto ActiveConnections = Manager_->GetActiveConnections();
    EXPECT_TRUE(ActiveConnections.empty());
    
    // Note: Creating actual connections would require a real server
    // This test verifies the API structure and error handling
    
    auto TestAddress = CreateTestAddress();
    ConnectionId Id;
    
    // Connection attempt should fail gracefully (no server running)
    auto Result = Manager_->CreateConnection(TestAddress, Id);
    // The result depends on the actual implementation - could be CONNECTION_FAILED
    // We're mainly testing that the API doesn't crash
    
    EXPECT_FALSE(Manager_->IsConnectionActive(999999)); // Non-existent connection
    EXPECT_EQ(Manager_->GetConnectionState(999999), ConnectionState::DISCONNECTED);
}

TEST_F(NetworkManagerTest, MessageQueueOperations)
{
    Manager_->Initialize(Config_);
    
    // Initially no messages
    EXPECT_FALSE(Manager_->HasIncomingMessages());
    EXPECT_EQ(Manager_->GetIncomingMessageCount(), 0);
    
    auto NextMessage = Manager_->GetNextMessage();
    EXPECT_EQ(NextMessage, nullptr);
    
    auto AllMessages = Manager_->GetAllMessages();
    EXPECT_TRUE(AllMessages.empty());
}

TEST_F(NetworkManagerTest, ConnectionGrouping)
{
    Manager_->Initialize(Config_);
    
    ConnectionId TestId = 123;
    std::string GroupName = "TestGroup";
    
    // Add connection to group
    auto Result = Manager_->AddConnectionToGroup(TestId, GroupName);
    EXPECT_EQ(Result, NetworkError::SUCCESS);
    
    // Get connections in group
    auto GroupConnections = Manager_->GetConnectionsInGroup(GroupName);
    EXPECT_EQ(GroupConnections.size(), 1);
    EXPECT_EQ(GroupConnections[0], TestId);
    
    // Remove connection from group
    Result = Manager_->RemoveConnectionFromGroup(TestId, GroupName);
    EXPECT_EQ(Result, NetworkError::SUCCESS);
    
    GroupConnections = Manager_->GetConnectionsInGroup(GroupName);
    EXPECT_TRUE(GroupConnections.empty());
    
    // Clear group
    Manager_->AddConnectionToGroup(TestId, GroupName);
    Manager_->ClearGroup(GroupName);
    GroupConnections = Manager_->GetConnectionsInGroup(GroupName);
    EXPECT_TRUE(GroupConnections.empty());
}

TEST_F(NetworkManagerTest, MessageHandlerCallbacks)
{
    Manager_->Initialize(Config_);
    
    std::atomic<bool> MessageReceived = false;
    std::atomic<bool> ConnectionChanged = false;
    
    // Set message handler
    Manager_->SetMessageHandler([&MessageReceived](const Message::Message& Msg) {
        MessageReceived = true;
    });
    
    // Set connection handler
    Manager_->SetConnectionHandler([&ConnectionChanged](ConnectionId Id, NetworkError Error) {
        ConnectionChanged = true;
    });
    
    // Remove all handlers
    Manager_->RemoveAllHandlers();
    
    // Handlers should be cleared successfully
    // (Testing actual handler invocation would require real network events)
}

TEST_F(NetworkManagerTest, ServerOperations)
{
    Manager_->Initialize(Config_);
    
    EXPECT_FALSE(Manager_->IsServerRunning());
    
    auto BindAddress = CreateTestAddress("0.0.0.0", 0); // Port 0 for auto-assignment
    
    // Starting server might fail due to permission issues, but should handle gracefully
    auto Result = Manager_->StartServer(BindAddress);
    
    // Stop server should always succeed
    auto StopResult = Manager_->StopServer();
    EXPECT_EQ(StopResult, NetworkError::SUCCESS);
    EXPECT_FALSE(Manager_->IsServerRunning());
}

TEST_F(NetworkManagerTest, NetworkStatistics)
{
    Manager_->Initialize(Config_);
    
    auto Stats = Manager_->GetNetworkStats();
    // Initial stats should be zero/empty
    EXPECT_EQ(Stats.TotalConnectionsCreated, 0);
    EXPECT_EQ(Stats.ActiveConnections, 0);
    EXPECT_EQ(Stats.TotalMessagesSent, 0);
    EXPECT_EQ(Stats.TotalMessagesReceived, 0);
    EXPECT_EQ(Stats.TotalBytesSent, 0);
    EXPECT_EQ(Stats.TotalBytesReceived, 0);
    
    // Get connection stats for non-existent connection
    auto ConnectionStats = Manager_->GetConnectionStats(999999);
    // Should return default/empty stats
    
    auto AllConnectionStats = Manager_->GetAllConnectionStats();
    EXPECT_TRUE(AllConnectionStats.empty());
}

TEST_F(NetworkManagerTest, MessageSendingOperations)
{
    Manager_->Initialize(Config_);
    
    // Create a test message
    auto TestMessage = Message::Message::Create(Message::MESSAGE_TYPE::GAME_STATE_UPDATE);
    TestMessage->SetPayload("Test message content");
    
    ConnectionId NonExistentId = 999999;
    
    // Sending to non-existent connection should fail appropriately
    auto Result = Manager_->SendMessage(NonExistentId, *TestMessage);
    EXPECT_EQ(Result, NetworkError::CONNECTION_NOT_FOUND);
    
    Result = Manager_->SendMessageReliable(NonExistentId, *TestMessage);
    EXPECT_EQ(Result, NetworkError::CONNECTION_NOT_FOUND);
    
    // Broadcasting with no connections should not crash
    Result = Manager_->BroadcastMessage(*TestMessage);
    // Result depends on implementation - could be SUCCESS (no connections to send to)
    
    Result = Manager_->BroadcastMessageToGroup("NonExistentGroup", *TestMessage);
    EXPECT_EQ(Result, NetworkError::GROUP_NOT_FOUND);
}

TEST_F(NetworkManagerTest, UtilityMethods)
{
    Manager_->Initialize(Config_);
    
    // Get connection info for non-existent connection
    auto Info = Manager_->GetConnectionInfo(999999);
    EXPECT_FALSE(Info.empty()); // Should return some default info
    
    // Get local addresses
    auto LocalAddresses = Manager_->GetLocalAddresses();
    // Should not crash, might be empty depending on implementation
    
    // Validate addresses
    auto TestAddress = CreateTestAddress();
    auto ValidationResult = Manager_->ValidateAddress(TestAddress);
    EXPECT_EQ(ValidationResult, NetworkError::SUCCESS);
}

TEST_F(NetworkManagerTest, CloseAllConnectionsWorks)
{
    Manager_->Initialize(Config_);
    
    // CloseAllConnections should work even with no connections
    auto Result = Manager_->CloseAllConnections();
    EXPECT_EQ(Result, NetworkError::SUCCESS);
    
    // Close specific non-existent connection
    Result = Manager_->CloseConnection(999999);
    EXPECT_EQ(Result, NetworkError::CONNECTION_NOT_FOUND);
}

TEST_F(NetworkManagerTest, ShutdownCleansUpProperly)
{
    Manager_->Initialize(Config_);
    EXPECT_TRUE(Manager_->IsInitialized());
    
    // Add some test data
    Manager_->AddConnectionToGroup(123, "TestGroup");
    
    // Shutdown should clean up everything
    Manager_->Shutdown();
    
    EXPECT_FALSE(Manager_->IsInitialized());
    EXPECT_FALSE(Manager_->IsServerRunning());
    
    // Operations after shutdown should fail appropriately
    ConnectionId Id;
    auto Address = CreateTestAddress();
    auto Result = Manager_->CreateConnection(Address, Id);
    EXPECT_EQ(Result, NetworkError::NOT_INITIALIZED);
}

TEST_F(NetworkManagerTest, MoveSemantics)
{
    Manager_->Initialize(Config_);
    Manager_->AddConnectionToGroup(123, "TestGroup");
    
    // Test move constructor
    auto MovedManager = std::move(*Manager_);
    
    // Original should be in moved-from state
    EXPECT_FALSE(Manager_->IsInitialized());
    
    // Moved-to should have the state
    // Note: This test verifies the move semantics don't crash
    // The exact behavior depends on implementation details
    
    Manager_.reset(); // Clean up the moved-from object
}

TEST_F(NetworkManagerTest, ThreadSafetyBasic)
{
    Manager_->Initialize(Config_);
    
    std::atomic<int> OperationCount = 0;
    
    // Multiple threads performing safe operations
    std::vector<std::thread> Threads;
    
    for (int i = 0; i < 5; ++i)
    {
        Threads.emplace_back([this, &OperationCount, i]() {
            Manager_->AddConnectionToGroup(i, "ThreadTestGroup");
            Manager_->GetActiveConnections();
            Manager_->GetNetworkStats();
            OperationCount++;
        });
    }
    
    for (auto& Thread : Threads)
    {
        Thread.join();
    }
    
    EXPECT_EQ(OperationCount.load(), 5);
    
    // Verify group has all connections
    auto GroupConnections = Manager_->GetConnectionsInGroup("ThreadTestGroup");
    EXPECT_EQ(GroupConnections.size(), 5);
}