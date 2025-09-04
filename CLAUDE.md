# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Current State vs Planned Architecture

**Helianthus core subsystems are production-ready** – message queue, configuration management, logging, metrics and CI/CD are implemented and tested. Microservices remain in planning; network layer has initial capabilities and tests.

## Build Commands

### Current Working Commands
```bash
# Build everything
bazel build //...

# Run all tests (Google Test integrated)
bazel test //...

# Run example
bazel run //Examples:PersistenceMetricsExample

# Build container image
docker build -f Containers/Dockerfile -t helianthus:latest .

# Run container
docker run --rm -p 8080:8080 helianthus:latest

# Clean build
bazel clean

# Code formatting
clang-format -i **/*.{cpp,hpp,h}
```

### Commands That Will Fail (Not Yet Implemented)
```bash
# Scripting not implemented
bazel build //... --define=ENABLE_LUA_SCRIPTING=1
```

## Architecture - What's Actually Built

### ✅ **Message Queue System** (Fully Implemented)
**Location**: `Shared/MessageQueue/`
- **IMessageQueue.h**: Interface for message queue operations
- **MessageQueue.h/cpp**: Core implementation with dead letter queue support
- **MessagePersistence.h/cpp**: Database persistence layer
- **Features**: MySQL/MongoDB backends, metrics collection, DLQ handling

### ✅ **Database Integration** (Working)
- **MySQL**: Via MariaDB connector (ThirdParty/mariadb-connector-c)
- **MongoDB**: Via mongo-cxx-driver (ThirdParty/mongo-cxx-driver)
- **Dual backend support** in MessagePersistence

### ✅ **Example Application**
**Location**: `Examples/PersistenceMetricsExample.cpp`
- Demonstrates end-to-end message queue usage
- Tests persistence and metrics functionality
- Serves as integration test

### ✅ **Configuration Management** (Fully Implemented)
**Location**: `Shared/Config/`
- Multi-source config (files, environment, CLI)
- Validators, change callbacks, templates, export (JSON/YAML/INI)
- Type-safe access, locking, deadlock protection, full test coverage

### ✅ **Logging System** (Fully Implemented)
**Tech**: spdlog-based structured logging
- Async logging, rotation, level control, JSON output, category loggers
- Trace/Span IDs, thread-safe, configurable formatter

### ✅ **Metrics & Monitoring** (Working)
- Prometheus metrics export, performance counters across subsystems

### ✅ **Testing & CI/CD** (Working)
- Google Test integrated; unit/integration tests pass 100%
- CI/CD pipeline and automated deployment are in place

### 🟡 **Network Layer** (Initial Implementation)
**Location**: `Shared/Network/`
- Basic TCP tests exist; API evolving

### ❌ **Missing Architecture** (Planned Only)
- **All Services**: Gateway, Auth, Game, Player, Match, Realtime, Monitor
- **Shared Components**: Network, Discovery, Protocol, Reflection, Scripting, Common
- **Tools**: BuildSystem, Monitoring (advanced)
- **Testing**: Advanced fault-injection scenarios

## Current Directory Structure

### What Actually Exists
```
Helianthus/
├── Examples/                    # Example applications
│   ├── BUILD.bazel
│   └── PersistenceMetricsExample.cpp
├── Shared/                      # Shared libraries
│   ├── BUILD.bazel
│   ├── MessageQueue/           # ✅ Working message queue
│   └── Network/                # 🟡 Initial network layer
├── ThirdParty/                 # Git submodules
│   ├── mariadb-connector-c/
│   └── mongo-cxx-driver/
├── message_queue_data/         # Runtime data directory
├── Tests/                      # ✅ Google Test-based tests
├── WORKSPACE                   # Bazel workspace
└── .gitmodules                # Submodule configuration
```

### What's Planned But Missing
```
Services/                       # All microservices
├── Gateway/
├── Auth/
├── Game/
├── Player/
├── Match/
├── Realtime/
└── Monitor/

Shared/                         # Most shared components
├── Discovery/
├── Protocol/
├── Reflection/
├── Scripting/
└── Common/

Tools/                          # Build and monitoring tools
Containers/                     # Dockerfiles and deployment manifests
```

## Development Workflow

### Current Development Cycle
1. **Build**: `bazel build //...`
2. **Test**: `bazel test //...`
3. **Run Example**: `bazel run //Examples:PersistenceMetricsExample`
4. **Format**: `clang-format -i **/*.{cpp,hpp,h}`
5. **Containerize**: `docker build -f Containers/Dockerfile -t helianthus:latest .`
6. **Verify**: Check `message_queue_data/`, Prometheus endpoints, container health

### Git Submodules
```bash
# Initialize/update submodules
git submodule update --init --recursive
```

## Development Phases (Current Status)
1. **Phase 1**: ✅ **COMPLETE** - Message queue infrastructure
2. **Phase 1.5**: ❌ **NOT STARTED** - Reflection system
3. **Phase 2**: ❌ **NOT STARTED** - Basic services
4. **Phase 3**: ❌ **NOT STARTED** - Game logic and real-time
5. **Phase 4**: ❌ **NOT STARTED** - Advanced features

Infrastructure subsystems (Configuration, Logging, Metrics, CI/CD) are ✅ COMPLETE. Network layer is 🟡 in progress.

## Key Implementation Details

### Message Queue Architecture
- **Persistence**: Messages stored in both MySQL and MongoDB
- **Metrics**: Performance tracking built into persistence layer
- **DLQ**: Dead letter queue for failed message processing
- **Example**: PersistenceMetricsExample.cpp demonstrates full usage

### Build Configuration
- **WORKSPACE**: Configured for Bazel with git submodules
- **BUILD.bazel**: 
  - `//Shared:MessageQueue` - Core library
  - `//Examples:PersistenceMetricsExample` - Demo application

## Planned Architecture (For Reference)

### Core Services (Not Yet Implemented)
- **Gateway Service**: API gateway and load balancer
- **Authentication Service**: Player authentication and session management
- **Game Logic Service**: Core game mechanics and rules
- **Player Service**: Player data and profile management
- **Match Service**: Matchmaking and game session management
- **Real-time Service**: WebSocket/TCP connections for real-time communication
- **Database Service**: Data persistence layer
- **Monitoring Service**: Metrics and health monitoring

### Technology Stack (Planned)
- **Language**: C++17/20
- **Build**: Bazel
- **Databases**: MySQL + MongoDB + Redis
- **Testing**: Google Test (implemented)
- **Scripting**: Lua/Python/JavaScript/C# (not implemented)
- **Networking**: Custom TCP/UDP (initial implementation)
- **Containerization**: Docker (planned)

## Coding Standards
- **Variables**: ALL variables must use UPPER_CASE naming convention
- **Parameters**: Use PascalCase (e.g., `GetPlayerData(PlayerId, PlayerName)`)
- **Variable Aliases**: Use PascalCase (e.g., `using ConnectionId = uint64_t`)
- **Functions/Methods**: Use PascalCase (e.g., `GetPlayerData()`, `ProcessMessage()`)
- **Classes**: Use PascalCase (e.g., `NetworkManager`, `GameSession`)
- **Interfaces**: Use PascalCase with `I` prefix (e.g., `IMessageHandler`, `INetworkSocket`)
- **Member Variables**: Use PascalCase (e.g., `PlayerId`, `ConnectionState`)
- **Enums/Macros**: Use UPPER_CASE (e.g., `CONNECTION_STATE_CONNECTED`, `HELIANTHUS_NETWORK_ENABLED`)
- **Namespaces**: Use PascalCase (e.g., `Helianthus::Network`, `Helianthus::Scripting`)
- **Directories**: Use PascalCase (e.g., `Services/Gateway`, `Shared/Network`)

## Next Development Priorities

### Immediate Tasks
1. **Containerization** (Dockerfiles, health checks, rollout strategy)
2. **Network layer enhancement** (TCP/UDP features, stability, performance)
3. **Service discovery mechanism**
4. **Basic service structure** (Gateway, Auth, Player)
5. **Reflection system foundation**
6. **Performance optimization** (serialization, batching, memory)