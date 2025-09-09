#include <gtest/gtest.h>

#include "Shared/Network/ConnectionManager.h"
#include "Shared/Network/NetworkTypes.h"

using namespace Helianthus::Network;

class ConnectionManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Don't initialize - just test construction
    }

    void TearDown() override
    {
        // Nothing to clean up
    }
};

// Test ConnectionManager construction
TEST_F(ConnectionManagerTest, Construction)
{
    // Just test that we can create a ConnectionManager
    ConnectionManager Manager;
    
    // Test that it's not initialized by default
    EXPECT_FALSE(Manager.IsInitialized());
    
    // Test that we can get the config
    NetworkConfig Config = Manager.GetConfig();
    EXPECT_EQ(Config.MaxConnections, 1000);
    EXPECT_EQ(Config.ConnectionTimeoutMs, 5000);
    EXPECT_TRUE(Config.EnableKeepalive);
    EXPECT_EQ(Config.KeepAliveIntervalMs, 30000);
}

// Test ConnectionManager initialization
TEST_F(ConnectionManagerTest, Initialization)
{
    ConnectionManager Manager;
    
    // Test initialization
    NetworkConfig Config;
    Config.MaxConnections = 100;
    Config.ConnectionTimeoutMs = 5000;
    Config.EnableKeepalive = true;
    
    NetworkError Result = Manager.Initialize(Config);
    EXPECT_EQ(Result, NetworkError::SUCCESS);
    EXPECT_TRUE(Manager.IsInitialized());
    
    // Test double initialization
    Result = Manager.Initialize(Config);
    EXPECT_EQ(Result, NetworkError::ALREADY_INITIALIZED);
    
    // Test shutdown
    Manager.Shutdown();
    EXPECT_FALSE(Manager.IsInitialized());
}
