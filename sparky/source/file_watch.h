#pragma once

#include "log.h"

#include <vector>
#include <codecvt>
#include <functional>
#include <filesystem>
#include <map>
#include <string>
#include <array>

#define NOMINMAX
#include <windows.h>

#define SP_FILE_WATCH_NOTIFY_FILTER FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_SECURITY

namespace detail
{
	struct sp_directory_watch
	{
		std::array<uint8_t, (sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR)) * 10> buffer = {};
		HANDLE file_handle;
		HANDLE completion_handle;
		std::map<std::string, std::vector<std::function<void(const char*)>>> file_watch_callbacks;
		OVERLAPPED overlapped;
	};

	std::map<std::string, sp_directory_watch> g_watched_directories;
}


void sp_file_watch_create(const char* filepath, std::function<void(const char*)>&& callback)
{
	std::filesystem::path file_watch_path(filepath);
	file_watch_path.make_preferred();
	std::string directory = file_watch_path.parent_path().string();
	std::string filename = file_watch_path.filename().string();

	if (detail::g_watched_directories.count(directory) == 0)
	{
		detail::sp_directory_watch& directory_watch = detail::g_watched_directories[directory];

		directory_watch.file_handle = CreateFile(
			directory.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			nullptr);

		directory_watch.completion_handle = CreateIoCompletionPort(
			directory_watch.file_handle,
			nullptr,
			reinterpret_cast<ULONG_PTR>(directory_watch.file_handle),
			1);

		BOOL result = ReadDirectoryChangesW(
			directory_watch.file_handle,
			directory_watch.buffer.data(),
			static_cast<DWORD>(directory_watch.buffer.size()),
			FALSE,
			SP_FILE_WATCH_NOTIFY_FILTER,
			nullptr,
			&directory_watch.overlapped,
			nullptr);
		assert(result == TRUE);
	}
		
	detail::sp_directory_watch& directory_watch = detail::g_watched_directories[directory];

	directory_watch.file_watch_callbacks[filename].push_back(callback);
}

void sp_file_watch_destroy()
{
#if 0
	CancelIo(g_file_handle);
	CloseHandle(g_file_handle);
	CloseHandle(g_completion_handle);
#endif
}

#include <iostream>

void sp_file_watch_tick()
{
	for (auto& dir : detail::g_watched_directories)
	{
		{
			DWORD transferred;
			ULONG_PTR key;
			LPOVERLAPPED lpoverlapped = nullptr;

			if (!GetQueuedCompletionStatus(dir.second.completion_handle, &transferred, &key, &lpoverlapped, 0))
			{
				continue;
			}
		}

		std::filesystem::path directory_watch_path(dir.first);

		const FILE_NOTIFY_INFORMATION* notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(dir.second.buffer.data());

		while (true)
		{
			std::string filename = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(notification->FileName, notification->FileName + notification->FileNameLength / sizeof(WCHAR));

			if (dir.second.file_watch_callbacks.count(filename))
			{
				sp_log("file change detected [%d]: %s", notification->Action, filename.c_str());

				std::filesystem::path file_watch_path = std::filesystem::path(dir.first) / std::filesystem::path(filename);
				std::string filepath = file_watch_path.string();

				for (auto& callback : dir.second.file_watch_callbacks[filename])
				{
					callback(filepath.c_str());
				}
			}

			if (notification->NextEntryOffset == 0)
			{
				break;
			}

			notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(reinterpret_cast<const char*>(notification) + notification->NextEntryOffset);
		}

		ReadDirectoryChangesW(
			dir.second.file_handle,
			dir.second.buffer.data(),
			static_cast<DWORD>(dir.second.buffer.size()),
			FALSE,
			SP_FILE_WATCH_NOTIFY_FILTER,
			nullptr,
			&dir.second.overlapped,
			nullptr);
	}
}