#pragma once

#include "RpcTypes.h"

#include <nlohmann/json.hpp>

#include <string>
#include <type_traits>

namespace Helianthus::RPC
{

/**
 * @brief RPC serialization interface
 */
class IRpcSerializer
{
public:
    virtual ~IRpcSerializer() = default;

    // Serialization
    virtual std::string Serialize(const void* Object, const std::string& TypeName) = 0;
    virtual bool Deserialize(const std::string& Data, void* Object, const std::string& TypeName) = 0;

    // Type-safe serialization
    template <typename T>
    std::string SerializeObject(const T& Object);

    template <typename T>
    bool DeserializeObject(const std::string& Data, T& Object);

    // Format support
    virtual SerializationFormat GetFormat() const = 0;
    virtual bool SupportsFormat(SerializationFormat Format) const = 0;
};

/**
 * @brief JSON-based RPC serializer
 */
class JsonRpcSerializer : public IRpcSerializer
{
public:
    JsonRpcSerializer();
    ~JsonRpcSerializer() override = default;

    // IRpcSerializer implementation
    std::string Serialize(const void* Object, const std::string& TypeName) override;
    bool Deserialize(const std::string& Data, void* Object, const std::string& TypeName) override;

    SerializationFormat GetFormat() const override
    {
        return SerializationFormat::JSON;
    }

    bool SupportsFormat(SerializationFormat Format) const override
    {
        return Format == SerializationFormat::JSON;
    }

private:
    nlohmann::json JsonObject;
};

/**
 * @brief Binary-based RPC serializer (placeholder)
 */
class BinaryRpcSerializer : public IRpcSerializer
{
public:
    BinaryRpcSerializer();
    ~BinaryRpcSerializer() override = default;

    // IRpcSerializer implementation
    std::string Serialize(const void* Object, const std::string& TypeName) override;
    bool Deserialize(const std::string& Data, void* Object, const std::string& TypeName) override;

    SerializationFormat GetFormat() const override
    {
        return SerializationFormat::BINARY;
    }

    bool SupportsFormat(SerializationFormat Format) const override
    {
        return Format == SerializationFormat::BINARY;
    }

private:
    std::string Buffer;
};

/**
 * @brief RPC serialization factory
 */
class RpcSerializerFactory
{
public:
    static std::unique_ptr<IRpcSerializer> CreateSerializer(SerializationFormat Format);
    static std::vector<SerializationFormat> GetSupportedFormats();
    static bool IsFormatSupported(SerializationFormat Format);
};

// Template implementations
template <typename T>
std::string IRpcSerializer::SerializeObject(const T& Object)
{
    return Serialize(&Object, typeid(T).name());
}

template <typename T>
bool IRpcSerializer::DeserializeObject(const std::string& Data, T& Object)
{
    return Deserialize(Data, &Object, typeid(T).name());
}

}  // namespace Helianthus::RPC
