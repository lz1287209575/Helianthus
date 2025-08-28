#include "Shared/MessageQueue/MessageQueue.h"

#include "Shared/Common/LogCategories.h"
#include "Shared/Common/StructuredLogger.h"

#include <shared_mutex>
#include <zlib.h>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <chrono>
#include <algorithm>

namespace Helianthus::MessageQueue
{

// 压缩和加密管理实现（拆分文件）
void MessageQueue::UpdateCompressionStats(const std::string& QueueName, uint64_t OriginalSize, uint64_t CompressedSize, double TimeMs)
{
    std::lock_guard<std::mutex> L(CompressionStatsMutex);
    auto& S = CompressionStatsData[QueueName];
    S.TotalMessages++;
    if (CompressedSize < OriginalSize)
    {
        S.CompressedMessages++;
        S.OriginalBytes += OriginalSize;
        S.CompressedBytes += CompressedSize;
        if (S.OriginalBytes > 0)
        {
            S.CompressionRatio = (S.CompressedBytes == 0) ? 0.0 : (double)S.CompressedBytes / (double)S.OriginalBytes;
        }
        S.AverageCompressionTimeMs = (S.AverageCompressionTimeMs * 0.9) + (TimeMs * 0.1);
    }
}

void MessageQueue::UpdateEncryptionStats(const std::string& QueueName, double TimeMs)
{
    std::lock_guard<std::mutex> L(EncryptionStatsMutex);
    auto& S = EncryptionStatsData[QueueName];
    S.TotalMessages++;
    S.EncryptedMessages++;
    S.AverageEncryptionTimeMs = (S.AverageEncryptionTimeMs * 0.9) + (TimeMs * 0.1);
}

void MessageQueue::UpdateDecryptionStats(const std::string& QueueName, double TimeMs)
{
    std::lock_guard<std::mutex> L(EncryptionStatsMutex);
    auto& S = EncryptionStatsData[QueueName];
    S.TotalMessages++;
    S.AverageDecryptionTimeMs = (S.AverageDecryptionTimeMs * 0.9) + (TimeMs * 0.1);
}

void MessageQueue::UpdateDecompressionStats(const std::string& QueueName, double TimeMs)
{
    std::lock_guard<std::mutex> L(CompressionStatsMutex);
    auto& S = CompressionStatsData[QueueName];
    S.TotalMessages++;
    S.AverageDecompressionTimeMs = (S.AverageDecompressionTimeMs * 0.9) + (TimeMs * 0.1);
}

QueueResult MessageQueue::ApplyCompression(MessagePtr Message, const std::string& QueueName)
{
    CompressionConfig Cfg{};
    GetCompressionConfig(QueueName, Cfg);
    if (!Cfg.EnableAutoCompression) return QueueResult::SUCCESS;
    if (Message->Payload.Data.size() < Cfg.MinSize) return QueueResult::SUCCESS;
    if (Message->Header.Properties.count("Compressed")) return QueueResult::SUCCESS;

    const auto Start = std::chrono::high_resolution_clock::now();
    const uint64_t Original = Message->Payload.Data.size();
    auto R = CompressMessage(Message, Cfg.Algorithm);
    const auto End = std::chrono::high_resolution_clock::now();
    const double Ms = std::chrono::duration<double, std::milli>(End - Start).count();
    if (R == QueueResult::SUCCESS)
    {
        UpdateCompressionStats(QueueName, Original, Message->Payload.Data.size(), Ms);
    }
    return R;
}

static bool IsLikelyGzip(const std::vector<char>& Data)
{
    return Data.size() >= 2 && static_cast<unsigned char>(Data[0]) == 0x1F && static_cast<unsigned char>(Data[1]) == 0x8B;
}

static bool IsLikelyZlib(const std::vector<char>& Data)
{
    if (Data.size() < 2) return false;
    unsigned char cmf = static_cast<unsigned char>(Data[0]);
    unsigned char flg = static_cast<unsigned char>(Data[1]);
    if ((cmf & 0x0F) != 8) return false; // DEFLATE
    return ((cmf << 8) + flg) % 31 == 0; // zlib 校验
}

QueueResult MessageQueue::ApplyDecompression(MessagePtr Message, const std::string& QueueName)
{
    if (!Message) return QueueResult::INVALID_PARAMETER;

    QueueResult R = QueueResult::SUCCESS;
    bool DidDecompress = false;
    auto Start = std::chrono::high_resolution_clock::now();

    // 首选标记触发
    if (Message->Header.Properties.count("Compressed"))
    {
        R = DecompressMessage(Message);
        DidDecompress = (R == QueueResult::SUCCESS);
    }
    else if (IsLikelyGzip(Message->Payload.Data) || IsLikelyZlib(Message->Payload.Data))
    {
        // 无标记但疑似GZIP，尝试解压
        const auto& In = Message->Payload.Data;
        size_t guess = In.size() * 8 + 1024;
        std::vector<char> Out(guess);
        uLongf dest_len = static_cast<uLongf>(Out.size());
        int rc = uncompress(reinterpret_cast<Bytef*>(Out.data()), &dest_len,
                            reinterpret_cast<const Bytef*>(In.data()), static_cast<uLongf>(In.size()));
        if (rc == Z_BUF_ERROR)
        {
            Out.resize(guess * 2);
            dest_len = static_cast<uLongf>(Out.size());
            rc = uncompress(reinterpret_cast<Bytef*>(Out.data()), &dest_len,
                            reinterpret_cast<const Bytef*>(In.data()), static_cast<uLongf>(In.size()));
        }
        if (rc == Z_OK)
        {
            Out.resize(static_cast<size_t>(dest_len));
            Message->Payload.Data.swap(Out);
            Message->Payload.Size = Message->Payload.Data.size();
            // 清理潜在旧标记
            Message->Header.Properties.erase("Compressed");
            Message->Header.Properties.erase("CompressionAlgorithm");
            DidDecompress = true;
            R = QueueResult::SUCCESS;
        }
        else
        {
            R = QueueResult::INTERNAL_ERROR;
        }
    }

    auto End = std::chrono::high_resolution_clock::now();
    if (DidDecompress)
    {
        UpdateDecompressionStats(QueueName, std::chrono::duration<double, std::milli>(End - Start).count());
    }
    return R;
}

QueueResult MessageQueue::ApplyEncryption(MessagePtr Message, const std::string& QueueName)
{
    EncryptionConfig Cfg{};
    GetEncryptionConfig(QueueName, Cfg);
    if (!Cfg.EnableAutoEncryption) return QueueResult::SUCCESS;
    if (Message->Header.Properties.count("Encrypted")) return QueueResult::SUCCESS;

    const auto Start = std::chrono::high_resolution_clock::now();
    auto R = EncryptMessage(Message, Cfg.Algorithm);
    const auto End = std::chrono::high_resolution_clock::now();
    const double Ms = std::chrono::duration<double, std::milli>(End - Start).count();
    if (R == QueueResult::SUCCESS)
    {
        UpdateEncryptionStats(QueueName, Ms);
    }
    return R;
}

QueueResult MessageQueue::ApplyDecryption(MessagePtr Message, const std::string& QueueName)
{
    if (!Message || !Message->Header.Properties.count("Encrypted")) return QueueResult::SUCCESS;
    auto Start = std::chrono::high_resolution_clock::now();
    auto R = DecryptMessage(Message);
    auto End = std::chrono::high_resolution_clock::now();
    if (R == QueueResult::SUCCESS)
    {
        UpdateDecryptionStats(QueueName, std::chrono::duration<double, std::milli>(End - Start).count());
    }
    return R;
}

// 额外启发式：疑似GCM打包则尝试解密
static bool LooksLikeGcmPacked(const std::vector<char>& Data)
{
    return Data.size() >= 28; // 12B nonce + >=0 ciphertext + 16B tag
}

QueueResult MessageQueue::ApplyDecryption(MessagePtr Message, const std::string& QueueName, int)
{
    if (!Message) return QueueResult::INVALID_PARAMETER;
    if (Message->Header.Properties.count("Encrypted"))
    {
        return ApplyDecryption(Message, QueueName);
    }
    if (LooksLikeGcmPacked(Message->Payload.Data))
    {
        // 临时设置标记以复用解密逻辑
        Message->Header.Properties["Encrypted"] = "1";
        Message->Header.Properties["EncryptionAlgorithm"] = "aes-256-gcm";
        auto R = ApplyDecryption(Message, QueueName);
        if (R != QueueResult::SUCCESS)
        {
            // 回滚标记
            Message->Header.Properties.erase("Encrypted");
            Message->Header.Properties.erase("EncryptionAlgorithm");
        }
        return R;
    }
    return QueueResult::SUCCESS;
}
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

    // 选取一个可用的配置（手动接口没有队列名）
    EncryptionConfig Cfg{};
    {
        std::shared_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
        if (!EncryptionConfigs.empty())
        {
            auto It = std::find_if(EncryptionConfigs.begin(), EncryptionConfigs.end(),
                                   [](const auto& P){ return P.second.EnableAutoEncryption; });
            if (It != EncryptionConfigs.end()) Cfg = It->second; else Cfg = EncryptionConfigs.begin()->second;
        }
    }

    if (Algorithm == EncryptionAlgorithm::AES_128_CBC)
    {
        uint8_t Key[16] = {0};
        uint8_t Iv[16] = {0};
        if (!Cfg.Key.empty()) memcpy(Key, Cfg.Key.data(), std::min<size_t>(Cfg.Key.size(), sizeof(Key)));
        if (!Cfg.IV.empty())  memcpy(Iv,  Cfg.IV.data(),  std::min<size_t>(Cfg.IV.size(),  sizeof(Iv)));
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
    else if (Algorithm == EncryptionAlgorithm::AES_256_GCM)
    {
        uint8_t Key[32] = {0};
        if (!Cfg.Key.empty()) memcpy(Key, Cfg.Key.data(), std::min<size_t>(Cfg.Key.size(), sizeof(Key)));

        uint8_t Nonce[12] = {0};
        if (Cfg.IV.size() >= 12) memcpy(Nonce, Cfg.IV.data(), 12); else RAND_bytes(Nonce, 12);

        EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
        if (!Ctx) return QueueResult::INTERNAL_ERROR;
        int Ok = EVP_EncryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_EncryptInit_ex(Ctx, nullptr, nullptr, Key, Nonce);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }

        std::vector<unsigned char> Cipher;
        Cipher.resize(Message->Payload.Data.size());
        int OutLen = 0, Tmp = 0;
        Ok = EVP_EncryptUpdate(Ctx, Cipher.data(), &OutLen,
                               reinterpret_cast<const unsigned char*>(Message->Payload.Data.data()),
                               static_cast<int>(Message->Payload.Data.size()));
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_EncryptFinal_ex(Ctx, Cipher.data() + OutLen, &Tmp);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        OutLen += Tmp;

        unsigned char Tag[16];
        Ok = EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_GET_TAG, 16, Tag);
        EVP_CIPHER_CTX_free(Ctx);
        if (Ok != 1) return QueueResult::INTERNAL_ERROR;

        Cipher.resize(static_cast<size_t>(OutLen));
        std::vector<unsigned char> Packed;
        Packed.reserve(12 + Cipher.size() + 16);
        Packed.insert(Packed.end(), Nonce, Nonce + 12);
        Packed.insert(Packed.end(), Cipher.begin(), Cipher.end());
        Packed.insert(Packed.end(), Tag, Tag + 16);

        Message->Payload.Data.assign(Packed.begin(), Packed.end());
        Message->Payload.Size = Message->Payload.Data.size();
        Message->Header.Properties["Encrypted"] = "1";
        Message->Header.Properties["EncryptionAlgorithm"] = "aes-256-gcm";
        Message->Header.Properties["GcmPacked"] = "nonce|ciphertext|tag";
        return QueueResult::SUCCESS;
    }

    return QueueResult::INVALID_PARAMETER;
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

    // 选取一个可用的配置（手动接口没有队列名）
    EncryptionConfig Cfg{};
    {
        std::shared_lock<std::shared_mutex> Lock(EncryptionConfigsMutex);
        if (!EncryptionConfigs.empty())
        {
            auto It = std::find_if(EncryptionConfigs.begin(), EncryptionConfigs.end(),
                                   [](const auto& P){ return P.second.EnableAutoEncryption; });
            if (It != EncryptionConfigs.end()) Cfg = It->second; else Cfg = EncryptionConfigs.begin()->second;
        }
    }

    if (Alg == "aes-128-cbc")
    {
        uint8_t Key[16] = {0};
        uint8_t Iv[16] = {0};
        if (!Cfg.Key.empty()) memcpy(Key, Cfg.Key.data(), std::min<size_t>(Cfg.Key.size(), sizeof(Key)));
        if (!Cfg.IV.empty())  memcpy(Iv,  Cfg.IV.data(),  std::min<size_t>(Cfg.IV.size(),  sizeof(Iv)));
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
    else if (Alg == "aes-256-gcm")
    {
        if (Message->Payload.Data.size() < 12 + 16) return QueueResult::INVALID_PARAMETER;
        const unsigned char* Ptr = reinterpret_cast<const unsigned char*>(Message->Payload.Data.data());
        const size_t Total = Message->Payload.Data.size();
        const unsigned char* Nonce = Ptr;
        const unsigned char* Tag = Ptr + (Total - 16);
        const unsigned char* Cipher = Ptr + 12;
        const size_t CipherLen = Total - 12 - 16;

        uint8_t Key[32] = {0};
        if (!Cfg.Key.empty()) memcpy(Key, Cfg.Key.data(), std::min<size_t>(Cfg.Key.size(), sizeof(Key)));

        EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
        if (!Ctx) return QueueResult::INTERNAL_ERROR;
        int Ok = EVP_DecryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_DecryptInit_ex(Ctx, nullptr, nullptr, Key, Nonce);
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }

        std::vector<unsigned char> Plain;
        Plain.resize(CipherLen);
        int OutLen = 0;
        Ok = EVP_DecryptUpdate(Ctx, Plain.data(), &OutLen, Cipher, static_cast<int>(CipherLen));
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        Ok = EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<unsigned char*>(Tag));
        if (Ok != 1) { EVP_CIPHER_CTX_free(Ctx); return QueueResult::INTERNAL_ERROR; }
        int FinalOk = EVP_DecryptFinal_ex(Ctx, Plain.data() + OutLen, &OutLen);
        EVP_CIPHER_CTX_free(Ctx);
        if (FinalOk != 1) return QueueResult::INTERNAL_ERROR;

        Message->Payload.Data.assign(Plain.begin(), Plain.end());
        Message->Payload.Size = Message->Payload.Data.size();
        Message->Header.Properties.erase("Encrypted");
        Message->Header.Properties.erase("EncryptionAlgorithm");
        Message->Header.Properties.erase("GcmPacked");
        return QueueResult::SUCCESS;
    }
    return QueueResult::SUCCESS;
}

} // namespace Helianthus::MessageQueue


