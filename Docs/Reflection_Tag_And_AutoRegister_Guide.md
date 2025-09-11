# 反射标签与自动注册使用指南

本文档介绍在 Helianthus 中使用反射业务标签、从函数签名推断 C++ 语义限定符，以及类级/全局控制工厂自动注册的方式，并给出按标签自动挂载的示例入口。

## 一、业务标签与签名限定符

- 业务标签（会进入 `MethodMeta.Tags`，用于筛选与说明）
  - 示例：`PureFunction`、`Math`、`Utility`、`Deprecated` 等
  - 写法：`HMETHOD(PureFunction,Math)` / `HMETHOD(PureFunction|Math)`

- C++ 语义限定符（不作为标签写入，由函数签名推断并记录到元数据布尔位）
  - 包含：`static`、`virtual`、`inline`、`const`、`noexcept`、`override`、`final`
  - 示例：
    - `HMETHOD(Math) static int Multiply(int a, int b) noexcept;`
    - `HMETHOD(Utility) virtual std::string GetType() const override;`

- 可选访问控制标签（仅当需要对外标注时使用）
  - `Public` / `Protected` / `Private` / `Friend`

## 二、类级控制：NoAutoRegister

- 在类上添加标签 `NoAutoRegister` 可禁止生成器为该类输出工厂自动注册（方法元信息仍会输出，用于标签筛选等场景）。
- 写法：
  - `HCLASS(MyTag,NoAutoRegister)`，例如：
    ```cpp
    HCLASS(MetaDemo,NoAutoRegister)
    class MetaExtensionDemo : public Helianthus::RPC::RpcServiceBase { ... };
    ```
- 典型用途：示例/测试中手动注册工厂，避免与全局链接单元的自动注册耦合。

## 三、全局开关：跳过所有类工厂自动注册（默认关闭）

- CMake 选项：`HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER`（默认 OFF）
- 开启方式：
  ```bash
  cmake -S . -B build -DHELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER=ON
  cmake --build build -j
  ```
- 效果：跳过所有类的工厂自动注册，仍输出方法元信息。可配合运行时手动注册工厂使用。

## 四、按标签自动挂载

- 入口函数：
  - `Helianthus::RPC::RegisterReflectedServices(IRpcServer& Server)`
  - `Helianthus::RPC::RegisterReflectedServices(IRpcServer& Server, const std::vector<std::string>& RequiredTags)`
- 行为：
  - 无标签版本：挂载注册表中的所有服务（若存在工厂）。
  - 带标签版本：仅挂载包含任一 RequiredTags 的方法的服务，并打印匹配方法名。

## 五、示例与生成器

- 示例：`Examples/MetaExtensionDemo.*`
  - 展示 `PureFunction/Math/Utility` 等业务标签
  - 展示 `NoAutoRegister` 类标签 + 手动注册工厂的使用
  - 展示按标签自动挂载（Math / Utility / PureFunction）输出

- 生成器：`Shared/Reflection/reflection_codegen.py`
  - 扫描 HCLASS/HPROPERTY/HMETHOD/HFUNCTION
  - 多标签解析：`TagA|TagB` / `TagA,TagB` -> 标签数组
  - 元信息：参数名、返回类型、可见性、描述（前置注释抽取）
  - C++ 语义限定符：自签名解析（不会写入 Tags）
  - 输出目录：`build/generated/`（含 `classes/*_registration.cpp` 与 `*_services.cpp`）

## 六、常见用法片段

- 业务标签 + 签名限定符：
  ```cpp
  // 可在脚本/RPC 对端调用的纯函数
  HMETHOD(PureFunction,Math) static int Multiply(int a, int b) noexcept;

  // 业务标签 Utility，语义从签名获取（inline/const）
  HMETHOD(Utility) inline int GetValue() const { return 42; }
  ```

- 类级 NoAutoRegister 与手动注册工厂：
  ```cpp
  HCLASS(Demo,NoAutoRegister)
  class DemoService : public Helianthus::RPC::RpcServiceBase { ... };

  // 运行时（例如示例程序中）手动注册：
  Helianthus::RPC::RpcServiceRegistry::Get().RegisterService(
      "DemoService", "1.0.0",
      [](){ return std::static_pointer_cast<Helianthus::RPC::IRpcService>(std::make_shared<DemoService>()); }
  );
  ```

- 按标签自动挂载：
  ```cpp
  std::vector<std::string> mathTags = {"Math"};
  Helianthus::RPC::RegisterReflectedServices(Server, mathTags);
  ```

## 七、注意事项

- 请仅将“业务含义”写入标签；C++ 语义由签名推断，避免把 `Static/Virtual/Const/...` 写入标签。
- 需要彻底禁用全局工厂自动注册时，使用 CMake 选项 `HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER=ON`。
- 调试生成结果可直接检查 `build/generated/classes/*_services.cpp` 与 `*_registration.cpp`。
