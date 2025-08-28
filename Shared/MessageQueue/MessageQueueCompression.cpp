#include "Shared/MessageQueue/MessageQueue.h"

#include "Shared/Common/LogCategories.h"
#include "Shared/Common/StructuredLogger.h"

#include <shared_mutex>
#include <zlib.h>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace Helianthus::MessageQueue
{

// 压缩和加密管理实现（拆分文件）
QueueResult MessageQueue::SetCompressionConfig(const std::string& QueueName, const CompressionConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(CompressionConfigsMutex);
    CompressionConfigs[QueueName] = Config;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
          "设置压缩配置: queue={}, algorithm={}, level={}, min_size={}",
          QueueName, static_cast<int>(Config.Algorithm), Config.Level, Config.MinSize);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetCompressionConfig(const std::string& QueueName, CompressionConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> Lock(CompressionConfigsMutex);
    auto It = CompressionConfigs.find(QueueName);
    if (It == CompressionConfigs.end())
    {
        OutConfig = CompressionConfig{};
        return QueueResult::SUCCESS;
    }
    OutConfig = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::SetEncryptionConfig(const std::string& QueueName, const EncryptionConfig& Config)
{
    std::unique_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
    EncryptionConfigs[QueueName] = Config;
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
          "设置加密配置: queue={}, algorithm={}, auto_encrypt={}",
          QueueName, static_cast<int>(Config.Algorithm), Config.EnableAutoEncryption);
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetEncryptionConfig(const std::string& QueueName, EncryptionConfig& OutConfig) const
{
    std::shared_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
    auto It = EncryptionConfigs.find(QueueName);
    if (It == EncryptionConfigs.end())
    {
        OutConfig = EncryptionConfig{};
        return QueueResult::SUCCESS;
    }
    OutConfig = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetCompressionStats(const std::string& QueueName, CompressionStats& OutStats) const
{
    std::lock_guard<std::mutex> Lock(CompressionStatsMutex);
    auto It = CompressionStatsData.find(QueueName);
    if (It == CompressionStatsData.end())
    {
        OutStats = CompressionStats{};
        return QueueResult::SUCCESS;
    }
    OutStats = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllCompressionStats(std::vector<CompressionStats>& OutStats) const
{
    std::lock_guard<std::mutex> Lock(CompressionStatsMutex);
    OutStats.clear();
    for (const auto& Pair : CompressionStatsData)
    {
        OutStats.push_back(Pair.second);
    }
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetEncryptionStats(const std::string& QueueName, EncryptionStats& OutStats) const
{
    std::lock_guard<std::mutex> Lock(EncryptionStatsMutex);
    auto It = EncryptionStatsData.find(QueueName);
    if (It == EncryptionStatsData.end())
    {
        OutStats = EncryptionStats{};
        return QueueResult::SUCCESS;
    }
    OutStats = It->second;
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::GetAllEncryptionStats(std::vector<EncryptionStats>& OutStats) const
{
    std::lock_guard<std::mutex> Lock(EncryptionStatsMutex);
    OutStats.clear();
    for (const auto& Pair : EncryptionStatsData)
    {
        OutStats.push_back(Pair.second);
    }
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::CompressMessage(MessagePtr Message, CompressionAlgorithm Algorithm)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
          "压缩消息: algorithm={}, size={}", static_cast<int>(Algorithm), Message->Payload.Data.size());
    if (Algorithm != CompressionAlgorithm::GZIP)
    {
        return QueueResult::INVALID_PARAMETER;
    }
    const auto& In = Message->Payload.Data;
    std::vector<char> Out;
    Out.resize(In.size() + In.size() / 16 + 64 + 3);
    uLongf dest_len = static_cast<uLongf>(Out.size());
    int level = 6;
    int rc = compress2(reinterpret_cast<Bytef*>(Out.data()), &dest_len,
                       reinterpret_cast<const Bytef*>(In.data()), static_cast<uLongf>(In.size()), level);
    if (rc != Z_OK)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    Out.resize(static_cast<size_t>(dest_len));
    Message->Payload.Data.swap(Out);
    Message->Payload.Size = Message->Payload.Data.size();
    Message->Header.Properties["Compressed"] = "1";
    Message->Header.Properties["CompressionAlgorithm"] = "gzip";
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DecompressMessage(MessagePtr Message)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
          "解压消息: size={}", Message->Payload.Data.size());
    const auto It = Message->Header.Properties.find("Compressed");
    if (It == Message->Header.Properties.end() || It->second != "1")
    {
        return QueueResult::SUCCESS;
    }
    const auto& In = Message->Payload.Data;
    size_t guess = In.size() * 4 + 1024;
    std::vector<char> Out(guess);
    uLongf dest_len = static_cast<uLongf>(Out.size());
    int rc = uncompress(reinterpret_cast<Bytef*>(Out.data()), &dest_len,
                        reinterpret_cast<const Bytef*>(In.data()), static_cast<uLongf>(In.size()));
    if (rc == Z_BUF_ERROR)
    {
        Out.resize(guess * 4);
        dest_len = static_cast<uLongf>(Out.size());
        rc = uncompress(reinterpret_cast<Bytef*>(Out.data()), &dest_len,
                        reinterpret_cast<const Bytef*>(In.data()), static_cast<uLongf>(In.size()));
    }
    if (rc != Z_OK)
    {
        return QueueResult::INTERNAL_ERROR;
    }
    Out.resize(static_cast<size_t>(dest_len));
    Message->Payload.Data.swap(Out);
    Message->Payload.Size = Message->Payload.Data.size();
    Message->Header.Properties.erase("Compressed");
    Message->Header.Properties.erase("CompressionAlgorithm");
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::EncryptMessage(MessagePtr Message, EncryptionAlgorithm Algorithm)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
          "加密消息: algorithm={}, size={}", static_cast<int>(Algorithm), Message->Payload.Data.size());
    if (Algorithm != EncryptionAlgorithm::AES_256_GCM && Algorithm != EncryptionAlgorithm::AES_128_CBC)
    {
        return QueueResult::INVALID_PARAMETER;
    }
    // OpenSSL EVP 实现（AES-128-CBC）
    if (Algorithm == EncryptionAlgorithm::AES_128_CBC)
    {
        uint8_t Key[16] = {0};
        uint8_t Iv[16] = {0};
        {
            std::shared_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
            auto It = EncryptionConfigs.find("");
            if (It != EncryptionConfigs.end())
            {
                const auto& C = It->second;
                memcpy(Key, C.Key.data(), std::min<size_t>(C.Key.size(), sizeof(Key)));
                memcpy(Iv, C.IV.data(), std::min<size_t>(C.IV.size(), sizeof(Iv)));
            }
        }
        EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
        if (!Ctx) return QueueResult::INTERNAL_ERROR;
        int Ok = EVP_EncryptInit_ex(Ctx, EVP_aes_128_cbc(), nullptr, Key, Iv);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        std::vector<unsigned char> Out;
        Out.resize(Message->Payload.Data.size() + EVP_CIPHER_block_size(EVP_aes_128_cbc()));
        int OutLen1 = 0, OutLen2 = 0;
        Ok = EVP_EncryptUpdate(Ctx, Out.data(), &OutLen1,
                               reinterpret_cast<const unsigned char*>(Message->Payload.Data.data()),
                               static_cast<int>(Message->Payload.Data.size()));
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_EncryptFinal_ex(Ctx, Out.data() + OutLen1, &OutLen2);
        EVP_CIPHER_CTX_free(Ctx);
        if (Ok != 1) return QueueResult::INTERNAL_ERROR;
        Out.resize(static_cast<size_t>(OutLen1 + OutLen2));
        Message->Payload.Data.assign(Out.begin(), Out.end());
        Message->Header.Properties["Encrypted"] = "1";
        Message->Header.Properties["EncryptionAlgorithm"] = "aes-128-cbc";
        return QueueResult::SUCCESS;
    }
    // TODO: AES_256_GCM 可后续接入 EVP_aes_256_gcm
    return QueueResult::SUCCESS;
}

QueueResult MessageQueue::DecryptMessage(MessagePtr Message)
{
    if (!Message || Message->Payload.Data.empty())
    {
        return QueueResult::INVALID_PARAMETER;
    }
    H_LOG(MQ, Helianthus::Common::LogVerbosity::Display,
          "解密消息: size={}", Message->Payload.Data.size());
    const auto It = Message->Header.Properties.find("Encrypted");
    if (It == Message->Header.Properties.end() || It->second != "1")
    {
        return QueueResult::SUCCESS;
    }
    const auto AlgIt = Message->Header.Properties.find("EncryptionAlgorithm");
    const std::string Alg = (AlgIt != Message->Header.Properties.end()) ? AlgIt->second : "";
    if (Alg == "aes-128-cbc")
    {
        uint8_t Key[16] = {0};
        uint8_t Iv[16] = {0};
        {
            std::shared_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
            auto CIt = EncryptionConfigs.find("");
            if (CIt != EncryptionConfigs.end())
            {
                const auto& C = CIt->second;
                memcpy(Key, C.Key.data(), std::min<size_t>(C.Key.size(), sizeof(Key)));
                memcpy(Iv, C.IV.data(), std::min<size_t>(C.IV.size(), sizeof(Iv)));
            }
        }
        EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
        if (!Ctx) return QueueResult::INTERNAL_ERROR;
        int Ok = EVP_DecryptInit_ex(Ctx, EVP_aes_128_cbc(), nullptr, Key, Iv);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        std::vector<unsigned char> Out;
        Out.resize(Message->Payload.Data.size());
        int OutLen1 = 0, OutLen2 = 0;
        Ok = EVP_DecryptUpdate(Ctx, Out.data(), &OutLen1,
                               reinterpret_cast<const unsigned char*>(Message->Payload.Data.data()),
                               static_cast<int>(Message->Payload.Data.size()));
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_DecryptFinal_ex(Ctx, Out.data() + OutLen1, &OutLen2);
        EVP_CIPHER_CTX_free(Ctx);
        if (Ok != 1) return QueueResult::INTERNAL_ERROR;
        Out.resize(static_cast<size_t>(OutLen1 + OutLen2));
        Message->Payload.Data.assign(Out.begin(), Out.end());
        Message->Header.Properties.erase("Encrypted");
        Message->Header.Properties.erase("EncryptionAlgorithm");
        return QueueResult::SUCCESS;
    }
    return QueueResult::SUCCESS;
}

} // namespace Helianthus::MessageQueue


