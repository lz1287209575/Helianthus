#include <gtest/gtest.h>
#include <iostream>

#include "Shared/Network/ConnectionManager.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Network;

class DebugConnectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::cout << "Setting up test..." << std::endl;
        
        // Initialize connection manager
        NetworkConfig NetworkConfig;
        NetworkConfig.MaxConnections = 100;
        NetworkConfig.ConnectionTimeoutMs = 5000;
        NetworkConfig.EnableKeepalive = true;
        
        std::cout << "Initializing ConnectionManager..." << std::endl;
        NetworkError Result = ConnectionManagerInstance.Initialize(NetworkConfig);
        ASSERT_EQ(Result, NetworkError::SUCCESS);
        std::cout << "ConnectionManager initialized successfully" << std::endl;
    }

    void TearDown() override
    {
        std::cout << "Tearing down test..." << std::endl;
        ConnectionManagerInstance.Shutdown();
        std::cout << "Test torn down" << std::endl;
    }

    ConnectionManager ConnectionManagerInstance;
};

// Test connection creation with debug output
TEST_F(DebugConnectionTest, DebugCreateConnection)
{
    std::cout << "Starting connection creation test..." << std::endl;
    
    // Create a connection
    NetworkAddress TestAddress("127.0.0.1", 8080);
    std::cout << "Created NetworkAddress: " << TestAddress.ToString() << std::endl;
    
    std::cout << "Calling CreateConnection..." << std::endl;
    ConnectionId ConnectionId = ConnectionManagerInstance.CreateConnection(TestAddress, ProtocolType::TCP);
    std::cout << "CreateConnection returned: " << ConnectionId << std::endl;
    
    EXPECT_NE(ConnectionId, InvalidConnectionId);
    EXPECT_EQ(ConnectionManagerInstance.GetConnectionCount(), 1);
    
    std::cout << "Test completed successfully" << std::endl;
}
