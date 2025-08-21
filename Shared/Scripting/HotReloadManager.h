#pragma once

#include "IScriptEngine.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Helianthus::Scripting
{
	class HotReloadManager
	{
	public:
		HotReloadManager();
		~HotReloadManager();

		void SetEngine(std::shared_ptr<IScriptEngine> EngineInstance);
		void SetPollIntervalMs(int IntervalMs);
		void SetFileExtensions(const std::vector<std::string>& Extensions);
		void SetOnFileReloaded(HotReloadCallback Callback);

		void AddWatchPath(const std::string& Path);
		void ClearWatchPaths();

		void Start();
		void Stop();

		bool IsRunning() const;

	private:
		void WatchLoop();
		void ScanPath(const std::string& Path);
		bool HasAllowedExtension(const std::string& Path) const;
		static std::string ToLower(std::string Value);

	private:
		std::shared_ptr<IScriptEngine> Engine;
		std::vector<std::string> WatchPaths;
		std::vector<std::string> Extensions{ ".lua" };
		std::unordered_map<std::string, std::filesystem::file_time_type> FileToWriteTime;
		HotReloadCallback Callback;

		std::atomic<bool> Running{ false };
		int PollIntervalMs{ 500 };
		std::thread Worker;
		mutable std::mutex Mutex;
	};
}


