#include "HotReloadManager.h"

#include <chrono>

namespace Helianthus::Scripting
{
HotReloadManager::HotReloadManager() = default;

HotReloadManager::~HotReloadManager()
{
    Stop();
}

void HotReloadManager::SetEngine(std::shared_ptr<IScriptEngine> EngineInstance)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    Engine = std::move(EngineInstance);
}

void HotReloadManager::SetPollIntervalMs(int IntervalMs)
{
    PollIntervalMs = IntervalMs;
}

void HotReloadManager::SetFileExtensions(const std::vector<std::string>& ExtensionsIn)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    Extensions = ExtensionsIn;
    for (auto& Ext : Extensions)
    {
        Ext = ToLower(Ext);
    }
}

void HotReloadManager::SetOnFileReloaded(HotReloadCallback CallbackIn)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    Callback = std::move(CallbackIn);
}

void HotReloadManager::AddWatchPath(const std::string& Path)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    WatchPaths.push_back(Path);
}

void HotReloadManager::ClearWatchPaths()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    WatchPaths.clear();
    FileToWriteTime.clear();
}

void HotReloadManager::Start()
{
    if (Running.exchange(true))
    {
        return;
    }
    Worker = std::thread([this]() { WatchLoop(); });
}

void HotReloadManager::Stop()
{
    if (!Running.exchange(false))
    {
        return;
    }
    if (Worker.joinable())
    {
        Worker.join();
    }
}

bool HotReloadManager::IsRunning() const
{
    return Running.load();
}

void HotReloadManager::WatchLoop()
{
    while (Running.load())
    {
        std::vector<std::string> PathsCopy;
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            PathsCopy = WatchPaths;
        }

        for (const auto& Path : PathsCopy)
        {
            ScanPath(Path);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(PollIntervalMs));
    }
}

void HotReloadManager::ScanPath(const std::string& Path)
{
    std::error_code Ec;
    for (auto It = std::filesystem::recursive_directory_iterator(Path, Ec);
         It != std::filesystem::recursive_directory_iterator();
         ++It)
    {
        if (Ec)
        {
            break;
        }
        const auto& Entry = *It;
        if (!Entry.is_regular_file())
        {
            continue;
        }
        const auto FilePath = Entry.path().string();
        if (!HasAllowedExtension(FilePath))
        {
            continue;
        }

        auto WriteTime = std::filesystem::last_write_time(Entry, Ec);
        if (Ec)
        {
            continue;
        }

        bool NeedReload = false;
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            auto ItMap = FileToWriteTime.find(FilePath);
            if (ItMap == FileToWriteTime.end())
            {
                FileToWriteTime[FilePath] = WriteTime;
            }
            else if (ItMap->second != WriteTime)
            {
                ItMap->second = WriteTime;
                NeedReload = true;
            }
        }

        if (NeedReload)
        {
            std::shared_ptr<IScriptEngine> EngineCopy;
            HotReloadCallback CallbackCopy;
            {
                std::lock_guard<std::mutex> Lock(Mutex);
                EngineCopy = Engine;
                CallbackCopy = Callback;
            }

            if (EngineCopy)
            {
                auto Result = EngineCopy->ReloadFile(FilePath);
                if (CallbackCopy)
                {
                    CallbackCopy(FilePath, Result.Success, Result.ErrorMessage);
                }
            }
        }
    }
}

bool HotReloadManager::HasAllowedExtension(const std::string& Path) const
{
    auto Lower = ToLower(Path);
    for (const auto& Ext : Extensions)
    {
        if (Lower.size() >= Ext.size() &&
            Lower.compare(Lower.size() - Ext.size(), Ext.size(), Ext) == 0)
        {
            return true;
        }
    }
    return false;
}

std::string HotReloadManager::ToLower(std::string Value)
{
    for (auto& Ch : Value)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    return Value;
}
}  // namespace Helianthus::Scripting
