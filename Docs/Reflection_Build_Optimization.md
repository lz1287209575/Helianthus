# 反射生成构建优化说明

本文档说明反射代码生成的两项构建优化：按目录分层输出与按类对象库聚合，帮助缩短增量编译与链接时间。

## 1. 分层输出目录

- 生成器将按源文件的相对路径分层输出：
  - 路径：`build/generated/classes/<相对源目录>/<ClassName>_registration.cpp`
  - 路径：`build/generated/classes/<相对源目录>/<ClassName>_services.cpp`
- 优点：
  - 便于快速定位某个类的生成片段
  - 为后续按目录或按类的并行构建提供基础

## 2. 按类对象库聚合（OBJECT Libraries）

- CMake 在 `Shared/Reflection/CMakeLists.txt` 中为每个类创建 OBJECT 库：
  - 目标名示例：`hel_refl_obj_<相对目录下划线替换>_<ClassName>`
  - 源文件：对应类的 `_registration.cpp` 与 `_services.cpp`
- 将每个 OBJECT 库的对象加入聚合静态库 `helianthus_reflection_gen`：
  - `target_sources(helianthus_reflection_gen PRIVATE $<TARGET_OBJECTS:...>)`
- 优点：
  - 类粒度的增量重建：单类变更仅重编译该类对象，减少整体链接压力
  - 与分层输出结合，定位与裁剪更清晰

## 3. 依赖与包含

- 生成的对象库与聚合库包含/链接：
  - `nlohmann_json::nlohmann_json`
  - `helianthus_common`
  - `helianthus_rpc`
- 统一包含路径：
  - 源码根：`${CMAKE_SOURCE_DIR}`
  - 生成目录：`${CMAKE_BINARY_DIR}/generated`

## 4. 与自动注册的关系

- 类标签 `NoAutoRegister`：跳过工厂注册，仅输出方法元信息
- 全局开关 `HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER`（默认 OFF）：跳过所有类工厂自动注册
- 二者都不影响对象库的生成与聚合；仅影响是否在运行时具备“创建服务”的工厂

## 5. 推荐使用

- 开发过程频繁修改单个类：受益于对象库增量重建
- 大量类同时变更：分层输出有助于并行编译与问题定位

## 6. 相关文档

- 反射系统使用指南：`Docs/REFLECTION_USAGE_GUIDE.md`
- 反射标签与自动注册使用指南：`Docs/Reflection_Tag_And_AutoRegister_Guide.md`
