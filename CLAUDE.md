# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Current State vs Planned Architecture

**Helianthus is in early development** - only the message queue infrastructure is fully implemented. The microservices architecture exists primarily as planning with solid foundations in place.

## Build Commands

### Current Working Commands
```bash
# Build everything (only message queue + examples)
bazel build //...

# Build specific components
bazel build //Shared:MessageQueue
bazel build //Examples:PersistenceMetricsExample

# Clean build
bazel clean

# Code formatting
clang-format -i **/*.{cpp,hpp,h}
```

### Commands That Will Fail (Not Yet Implemented)
```bash
bazel build //Shared/Network:network      # Network layer missing
bazel test //...                          # No tests implemented
bazel test //Tests/Network:all            # Tests directory missing
bazel build //... --define=ENABLE_LUA_SCRIPTING=1  # Scripting not implemented
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

### ❌ **Missing Architecture** (Planned Only)
- **All Services**: Gateway, Auth, Game, Player, Match, Realtime, Monitor
- **Shared Components**: Network, Discovery, Protocol, Reflection, Scripting, Common
- **Tools**: BuildSystem, Monitoring
- **Testing**: Google Test not implemented

## Current Directory Structure

### What Actually Exists
```
Helianthus/
├── Examples/                    # Example applications
│   ├── BUILD.bazel
│   └── PersistenceMetricsExample.cpp
├── Shared/                      # Shared libraries
│   ├── BUILD.bazel
│   └── MessageQueue/           # ✅ Working message queue
├── ThirdParty/                 # Git submodules
│   ├── mariadb-connector-c/
│   └── mongo-cxx-driver/
├── message_queue_data/         # Runtime data directory
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
├── Network/
├── Discovery/
├── Protocol/
├── Reflection/
├── Scripting/
└── Common/

Tools/                          # Build and monitoring tools
Tests/                          # Google Test framework
```

## Development Workflow

### Current Development Cycle
1. **Build**: `bazel build //...`
2. **Test**: `bazel run //Examples:PersistenceMetricsExample`
3. **Format**: `clang-format -i **/*.{cpp,hpp,h}`
4. **Verify**: Check message_queue_data/ for runtime artifacts

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
- **Testing**: Google Test (not implemented)
- **Scripting**: Lua/Python/JavaScript/C# (not implemented)
- **Networking**: Custom TCP/UDP (not implemented)
- **Containerization**: Docker (not implemented)

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
1. **Network layer implementation** (TCP/UDP)
2. **Google Test integration** and test framework
3. **Service discovery mechanism**
4. **Basic service structure** (Gateway, Auth, Player)
5. **Reflection system foundation**