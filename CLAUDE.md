# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Helianthus is a microservices-based game server architecture designed for scalability and modularity.

## Common Commands
- **Build all**: `bazel build //...`
- **Build specific target**: `bazel build //Shared/Network:network`
- **Run tests**: `bazel test //...`
- **Test specific component**: `bazel test //Tests/Network:all`
- **Format code**: `clang-format -i **/*.{cpp,hpp,h}`
- **Enable scripting**: `bazel build //... --define=ENABLE_LUA_SCRIPTING=1`
- **Clean build**: `bazel clean`

## Architecture Overview
This is a microservices game server with the following planned structure:

### Core Services (Planned)
- **Gateway Service**: API gateway and load balancer
- **Authentication Service**: Player authentication and session management
- **Game Logic Service**: Core game mechanics and rules
- **Player Service**: Player data and profile management
- **Match Service**: Matchmaking and game session management
- **Real-time Service**: WebSocket/TCP connections for real-time communication
- **Database Service**: Data persistence layer
- **Monitoring Service**: Metrics and health monitoring

### Technology Stack
- Programming Language: **C++** (C++17/20)
- Build System: **Bazel** (scalable, multi-language support)
- Dependency Management: **Git Submodules** (manual management)
- Database: **MySQL** (relational data) + **MongoDB** (document data) + Redis (cache)
- Networking: **Custom Implementation** (high-performance TCP/UDP)
- Message Queue: **Custom Implementation** (inter-service communication)
- Service Discovery: **Custom Implementation** (service registry and discovery)
- Load Balancing: **Custom Implementation** (application-level)
- Serialization: **Protocol Buffers** + **Custom Binary Protocol**
- Container: **Docker**
- CI/CD: **Custom Web-based Build System**
- Logging: **spdlog** (transitioning to custom implementation)
- Testing: **Google Test** (unit and integration tests)
- Code Formatting: **clang-format**
- Monitoring: **Custom Implementation** (metrics and health checks)
- Log Collection: **ELK Stack** (Elasticsearch, Logstash, Kibana)
- Runtime Reflection: **Custom Implementation** (full reflection + property system + serialization)
- Scripting: **Multi-language Support** (Lua/Python/JavaScript/C# via macro control)
  - Hot-reload support for configuration, AI behavior, game logic

## Development Workflow
### Build and Test
- Use Bazel for all builds: `bazel build //...`
- Run tests: `bazel test //...`
- Format code before commit: `clang-format -i **/*.{cpp,hpp,h}`

### Project Structure (Planned)
```
Services/
├── Gateway/          # API Gateway Service
├── Auth/            # Authentication Service  
├── Game/            # Game Logic Service
├── Player/          # Player Management Service
├── Match/           # Matchmaking Service
├── Realtime/        # Real-time Communication Service
└── Monitor/         # Monitoring Service

Shared/
├── Network/         # Custom networking implementation
├── Message/         # Custom message queue
├── Discovery/       # Service discovery
├── Protocol/        # Protobuf definitions + custom binary
├── Reflection/      # Runtime reflection system
├── Scripting/       # Multi-language scripting engine
└── Common/          # Shared utilities

Tools/
├── BuildSystem/     # Custom CI/CD web tools
└── Monitoring/      # Custom monitoring implementation
```

## Development Phases
1. **Phase 1**: Core infrastructure (networking, message queue, service discovery)
2. **Phase 1.5**: Reflection system and scripting engine foundation
3. **Phase 2**: Basic services (gateway, auth, player) with reflection integration
4. **Phase 3**: Game logic, real-time communication with script hot-reload
5. **Phase 4**: Advanced features (monitoring, custom logging, editor support)

## Key Features
### Runtime Reflection System
- **Class Reflection**: Full class metadata with inheritance support
- **Member Reflection**: Properties, fields, methods with type information
- **Property System**: UE-style properties with getters/setters/validation
- **Serialization Integration**: Automatic serialization via reflection
- **Script Binding**: Auto-generated bindings for all script languages
- **Editor Support**: Runtime inspection and modification

### Multi-Language Scripting
- **Supported Languages**: Lua, Python, JavaScript (V8), C# (.NET)
- **Macro Control**: `#ifdef ENABLE_LUA_SCRIPTING` etc.
- **Hot Reload**: Runtime script reloading without service restart
- **Use Cases**: Configuration loading, AI behavior trees, game logic
- **Performance**: Compiled scripts with JIT where possible

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

## Development Notes
- All custom implementations should prioritize performance and scalability
- Use Git submodules for third-party dependencies  
- Gradual migration from spdlog to custom logging system
- Docker containerization for all services
- Reflection macros should be minimal and non-intrusive
- Script engines should be sandboxed for security