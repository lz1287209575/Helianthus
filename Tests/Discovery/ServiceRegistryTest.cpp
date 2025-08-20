#include <chrono>
#include <thread>

#include "Discovery/DiscoveryTypes.h"
#include "Discovery/ServiceRegistry.h"
#include <gtest/gtest.h>

using namespace Helianthus::Discovery;

class ServiceRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Config_.MaxServices = 100;
        Config_.DefaultTtlMs = 30000;      // 30 seconds
        Config_.CleanupIntervalMs = 1000;  // 1 second
        Config_.EnablePersistence = false;
        Config_.EnableReplication = false;

        Registry_ = std::make_unique<ServiceRegistry>();
    }

    void TearDown() override
    {
        if (Registry_)
        {
            Registry_->Shutdown();
        }
    }

    ServiceInstance CreateTestService(const std::string& ServiceName,
                                      const std::string& Host = "localhost",
                                      uint16_t Port = 8080)
    {
        ServiceInstance Instance;
        Instance.BaseInfo.ServiceName = ServiceName;
        Instance.BaseInfo.ServiceVersion = "1.0.0";
        Instance.BaseInfo.HostAddress = Host;
        Instance.BaseInfo.Port = Port;
        Instance.State = ServiceState::HEALTHY;
        Instance.CurrentHealthScore = 100;

        Instance.ActiveConnections = 0;
        Instance.MaxConnections = 1000;
        Instance.Region = "us-west-1";
        Instance.Zone = "us-west-1a";
        Instance.Tags["environment"] = "test";
        Instance.Tags["version"] = "1.0.0";

        return Instance;
    }

    RegistryConfig Config_;
    std::unique_ptr<ServiceRegistry> Registry_;
};

TEST_F(ServiceRegistryTest, InitializationWorksCorrectly)
{
    EXPECT_FALSE(Registry_->IsInitialized());

    auto Result = Registry_->Initialize(Config_);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);
    EXPECT_TRUE(Registry_->IsInitialized());

    // Double initialization should fail
    Result = Registry_->Initialize(Config_);
    EXPECT_EQ(Result, DiscoveryResult::INTERNAL_ERROR);
}

TEST_F(ServiceRegistryTest, ServiceRegistrationWorksCorrectly)
{
    Registry_->Initialize(Config_);

    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;

    auto Result = Registry_->RegisterService(TestService, InstanceId);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);
    EXPECT_NE(InstanceId, 0);

    // Verify service was registered
    auto RetrievedService = Registry_->GetService(InstanceId);
    EXPECT_NE(RetrievedService, nullptr);
    EXPECT_EQ(RetrievedService->BaseInfo.ServiceName, "TestService");
    EXPECT_EQ(RetrievedService->InstanceId, InstanceId);
}

TEST_F(ServiceRegistryTest, ServiceDeregistrationWorksCorrectly)
{
    Registry_->Initialize(Config_);

    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;

    Registry_->RegisterService(TestService, InstanceId);
    EXPECT_NE(Registry_->GetService(InstanceId), nullptr);

    // Deregister service
    auto Result = Registry_->DeregisterService(InstanceId);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);

    // Service should no longer exist
    auto RetrievedService = Registry_->GetService(InstanceId);
    EXPECT_EQ(RetrievedService, nullptr);
}

TEST_F(ServiceRegistryTest, ServiceLookupByNameWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register multiple instances of the same service
    auto Service1 = CreateTestService("WebService", "host1", 8080);
    auto Service2 = CreateTestService("WebService", "host2", 8081);
    auto Service3 = CreateTestService("DatabaseService", "dbhost", 5432);

    ServiceInstanceId Id1, Id2, Id3;
    Registry_->RegisterService(Service1, Id1);
    Registry_->RegisterService(Service2, Id2);
    Registry_->RegisterService(Service3, Id3);

    // Get services by name
    auto WebServices = Registry_->GetServicesByName("WebService");
    EXPECT_EQ(WebServices.size(), 2);

    auto DbServices = Registry_->GetServicesByName("DatabaseService");
    EXPECT_EQ(DbServices.size(), 1);

    auto NonExistentServices = Registry_->GetServicesByName("NonExistent");
    EXPECT_EQ(NonExistentServices.size(), 0);
}

TEST_F(ServiceRegistryTest, HealthyServiceFilteringWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register services with different health states
    auto HealthyService = CreateTestService("WebService", "host1", 8080);
    HealthyService.State = ServiceState::HEALTHY;

    auto UnhealthyService = CreateTestService("WebService", "host2", 8081);
    UnhealthyService.State = ServiceState::UNHEALTHY;

    ServiceInstanceId Id1, Id2;
    Registry_->RegisterService(HealthyService, Id1);
    Registry_->RegisterService(UnhealthyService, Id2);

    // Get all services
    auto AllServices = Registry_->GetServicesByName("WebService");
    EXPECT_EQ(AllServices.size(), 2);

    // Get only healthy services
    auto HealthyServices = Registry_->GetHealthyServices("WebService");
    EXPECT_EQ(HealthyServices.size(), 1);
    EXPECT_EQ(HealthyServices[0]->InstanceId, Id1);
}

TEST_F(ServiceRegistryTest, ServiceUpdateWorksCorrectly)
{
    Registry_->Initialize(Config_);

    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;
    Registry_->RegisterService(TestService, InstanceId);

    // Update service information
    TestService.BaseInfo.Port = 9090;
    TestService.ActiveConnections = 50;
    TestService.Tags["updated"] = "true";

    auto Result = Registry_->UpdateService(InstanceId, TestService);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);

    // Verify update
    auto UpdatedService = Registry_->GetService(InstanceId);
    EXPECT_NE(UpdatedService, nullptr);
    EXPECT_EQ(UpdatedService->BaseInfo.Port, 9090);
    EXPECT_EQ(UpdatedService->ActiveConnections, 50);
    EXPECT_EQ(UpdatedService->Tags["updated"], "true");
}

TEST_F(ServiceRegistryTest, ServiceStateUpdateWorksCorrectly)
{
    Registry_->Initialize(Config_);

    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;
    Registry_->RegisterService(TestService, InstanceId);

    // Update service state
    auto Result = Registry_->UpdateServiceState(InstanceId, ServiceState::MAINTENANCE);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);

    // Verify state change
    auto State = Registry_->GetServiceState(InstanceId);
    EXPECT_EQ(State, ServiceState::MAINTENANCE);
}

TEST_F(ServiceRegistryTest, HeartbeatWorksCorrectly)
{
    Registry_->Initialize(Config_);

    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;
    Registry_->RegisterService(TestService, InstanceId);

    // Send heartbeat
    auto Result = Registry_->SendHeartbeat(InstanceId);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);

    // Heartbeat for non-existent service should fail
    Result = Registry_->SendHeartbeat(999999);
    EXPECT_EQ(Result, DiscoveryResult::SERVICE_NOT_FOUND);
}

TEST_F(ServiceRegistryTest, ServiceCountingWorksCorrectly)
{
    Registry_->Initialize(Config_);

    EXPECT_EQ(Registry_->GetServiceCount(), 0);
    EXPECT_EQ(Registry_->GetServiceInstanceCount(), 0);

    // Register services
    auto Service1 = CreateTestService("WebService");
    auto Service2 = CreateTestService("WebService");
    auto Service3 = CreateTestService("DatabaseService");

    ServiceInstanceId Id1, Id2, Id3;
    Registry_->RegisterService(Service1, Id1);
    Registry_->RegisterService(Service2, Id2);
    Registry_->RegisterService(Service3, Id3);

    EXPECT_EQ(Registry_->GetServiceCount(), 2);          // 2 unique service names
    EXPECT_EQ(Registry_->GetServiceInstanceCount(), 3);  // 3 instances total
}

TEST_F(ServiceRegistryTest, ServiceNamesRetrievalWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register services with different names
    auto WebService = CreateTestService("WebService");
    auto DbService = CreateTestService("DatabaseService");
    auto AuthService = CreateTestService("AuthService");

    ServiceInstanceId Id1, Id2, Id3;
    Registry_->RegisterService(WebService, Id1);
    Registry_->RegisterService(DbService, Id2);
    Registry_->RegisterService(AuthService, Id3);

    auto ServiceNames = Registry_->GetServiceNames();
    EXPECT_EQ(ServiceNames.size(), 3);

    // Names should be present (order not guaranteed)
    auto HasName = [&ServiceNames](const std::string& Name)
    { return std::find(ServiceNames.begin(), ServiceNames.end(), Name) != ServiceNames.end(); };

    EXPECT_TRUE(HasName("WebService"));
    EXPECT_TRUE(HasName("DatabaseService"));
    EXPECT_TRUE(HasName("AuthService"));
}

TEST_F(ServiceRegistryTest, TagBasedSearchWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register services with different tags
    auto ProdService = CreateTestService("WebService");
    ProdService.Tags["environment"] = "production";
    ProdService.Tags["tier"] = "frontend";

    auto TestService = CreateTestService("WebService");
    TestService.Tags["environment"] = "test";
    TestService.Tags["tier"] = "frontend";

    ServiceInstanceId Id1, Id2;
    Registry_->RegisterService(ProdService, Id1);
    Registry_->RegisterService(TestService, Id2);

    // Search by tag
    auto ProdServices = Registry_->FindServicesByTag("environment", "production");
    EXPECT_EQ(ProdServices.size(), 1);
    EXPECT_EQ(ProdServices[0]->InstanceId, Id1);

    auto FrontendServices = Registry_->FindServicesByTag("tier", "frontend");
    EXPECT_EQ(FrontendServices.size(), 2);
}

TEST_F(ServiceRegistryTest, RegionBasedSearchWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register services in different regions
    auto UsService = CreateTestService("WebService");
    UsService.Region = "us-west-1";

    auto EuService = CreateTestService("WebService");
    EuService.Region = "eu-west-1";

    ServiceInstanceId Id1, Id2;
    Registry_->RegisterService(UsService, Id1);
    Registry_->RegisterService(EuService, Id2);

    // Search by region
    auto UsServices = Registry_->FindServicesByRegion("us-west-1");
    EXPECT_EQ(UsServices.size(), 1);
    EXPECT_EQ(UsServices[0]->Region, "us-west-1");

    auto EuServices = Registry_->FindServicesByRegion("eu-west-1");
    EXPECT_EQ(EuServices.size(), 1);
    EXPECT_EQ(EuServices[0]->Region, "eu-west-1");
}

TEST_F(ServiceRegistryTest, AllServicesRetrievalWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register multiple services
    for (int i = 0; i < 5; ++i)
    {
        auto Service = CreateTestService("Service" + std::to_string(i));
        ServiceInstanceId InstanceId;
        Registry_->RegisterService(Service, InstanceId);
    }

    auto AllServices = Registry_->GetAllServices();
    EXPECT_EQ(AllServices.size(), 5);
}

TEST_F(ServiceRegistryTest, DeregisterByNameWorksCorrectly)
{
    Registry_->Initialize(Config_);

    // Register multiple instances of the same service
    auto Service1 = CreateTestService("WebService", "host1", 8080);
    auto Service2 = CreateTestService("WebService", "host2", 8081);
    auto Service3 = CreateTestService("DatabaseService", "dbhost", 5432);

    ServiceInstanceId Id1, Id2, Id3;
    Registry_->RegisterService(Service1, Id1);
    Registry_->RegisterService(Service2, Id2);
    Registry_->RegisterService(Service3, Id3);

    EXPECT_EQ(Registry_->GetServicesByName("WebService").size(), 2);

    // Deregister all instances of WebService
    auto Result = Registry_->DeregisterServiceByName("WebService");
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);

    EXPECT_EQ(Registry_->GetServicesByName("WebService").size(), 0);
    EXPECT_EQ(Registry_->GetServicesByName("DatabaseService").size(), 1);  // Should remain
}

TEST_F(ServiceRegistryTest, RegistryStatsWorksCorrectly)
{
    Registry_->Initialize(Config_);

    auto Stats = Registry_->GetRegistryStats();
    EXPECT_EQ(Stats.TotalServices, 0);
    EXPECT_EQ(Stats.TotalServiceInstances, 0);
    EXPECT_EQ(Stats.RegistrationCount, 0);

    // Register a service
    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;
    Registry_->RegisterService(TestService, InstanceId);

    Stats = Registry_->GetRegistryStats();
    EXPECT_EQ(Stats.RegistrationCount, 1);
    EXPECT_EQ(Stats.TotalServices, 1);
    EXPECT_EQ(Stats.TotalServiceInstances, 1);

    // Deregister the service
    Registry_->DeregisterService(InstanceId);

    Stats = Registry_->GetRegistryStats();
    EXPECT_EQ(Stats.DeregistrationCount, 1);
    EXPECT_EQ(Stats.TotalServices, 0);
    EXPECT_EQ(Stats.TotalServiceInstances, 0);
}

TEST_F(ServiceRegistryTest, MaintenanceModeWorksCorrectly)
{
    Registry_->Initialize(Config_);

    EXPECT_FALSE(Registry_->IsInMaintenanceMode());

    // Enable maintenance mode
    Registry_->SetMaintenanceMode(true);
    EXPECT_TRUE(Registry_->IsInMaintenanceMode());

    // Registration should fail in maintenance mode
    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;
    auto Result = Registry_->RegisterService(TestService, InstanceId);
    EXPECT_EQ(Result, DiscoveryResult::INTERNAL_ERROR);

    // Disable maintenance mode
    Registry_->SetMaintenanceMode(false);
    EXPECT_FALSE(Registry_->IsInMaintenanceMode());

    // Registration should work again
    Result = Registry_->RegisterService(TestService, InstanceId);
    EXPECT_EQ(Result, DiscoveryResult::SUCCESS);
}

TEST_F(ServiceRegistryTest, InvalidServiceRegistrationFails)
{
    Registry_->Initialize(Config_);

    // Service with empty name should fail
    ServiceInstance InvalidService;
    InvalidService.BaseInfo.ServiceName = "";  // Empty name

    ServiceInstanceId InstanceId;
    auto Result = Registry_->RegisterService(InvalidService, InstanceId);
    EXPECT_EQ(Result, DiscoveryResult::INVALID_SERVICE_INFO);
}

TEST_F(ServiceRegistryTest, ShutdownCleansUpProperly)
{
    Registry_->Initialize(Config_);

    // Register some services
    auto TestService = CreateTestService("TestService");
    ServiceInstanceId InstanceId;
    Registry_->RegisterService(TestService, InstanceId);

    EXPECT_EQ(Registry_->GetServiceInstanceCount(), 1);
    EXPECT_TRUE(Registry_->IsInitialized());

    // Shutdown should clean up everything
    Registry_->Shutdown();

    EXPECT_FALSE(Registry_->IsInitialized());
    EXPECT_EQ(Registry_->GetServiceInstanceCount(), 0);
}