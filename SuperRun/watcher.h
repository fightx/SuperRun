#pragma once

#include "util.h"
#include "element.h"

#include <boost/filesystem.hpp>
#include <boost/range/algorithm/count.hpp>

bool WatchDirectory(super_run_list *list, const std::wstring& fullpath, int level, const HANDLE exit_event, const HANDLE ready_event)
{
	boost::filesystem::wpath directory(fullpath);

	HANDLE dir = ::CreateFileW(fullpath.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
	if (dir == INVALID_HANDLE_VALUE)
	{
		SetEvent(ready_event);
		return false;
	}

	char notify[256 * (sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR))] = { 0 };
	DWORD dwBytes;
	OVERLAPPED overlapped = {};
	overlapped.hEvent = ::CreateEvent(0, FALSE, FALSE, 0);

	boost::filesystem::wpath oldpath;
	while (1)
	{
		::ReadDirectoryChangesW(dir, &notify, sizeof(notify), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME, &dwBytes, &overlapped, NULL);
		SetEvent(ready_event);

		HANDLE event[2] = { exit_event, overlapped.hEvent };
		int index = ::WaitForMultipleObjects(2, event, FALSE, INFINITE);
		if (index == WAIT_OBJECT_0) break;

		FILE_NOTIFY_INFORMATION *pnotify = (FILE_NOTIFY_INFORMATION *)notify;

		while (1)
		{
			std::wstring file = std::wstring(pnotify->FileName, 0, pnotify->FileNameLength / sizeof(WCHAR));
			int directory_level = boost::count(file, boost::filesystem::wpath::preferred_separator);

			boost::filesystem::wpath path = directory / file;

			if (directory_level <= level && pnotify->Action == FILE_ACTION_RENAMED_OLD_NAME)
			{
				oldpath = path;
			}
			
			boost::system::error_code ec;
			if (directory_level <= level)
			{
				switch (pnotify->Action)
				{
				case FILE_ACTION_ADDED:
					//增加文件
					list->append_file(path);
					break;
				case FILE_ACTION_REMOVED:
					//删除文件
					list->remove_file(path.wstring());
					break;
				case FILE_ACTION_RENAMED_NEW_NAME:
					if (boost::filesystem::is_directory(path, ec))
					{
						if (directory_level < level)
						{
							//文件夹重命名
							list->rename_path(oldpath.wstring(), path.wstring());
						}
					}
					else
					{
						//文件重命名
						list->rename_path(oldpath.wstring(), path.wstring());
					}
					break;
				default:
					break;
				}
			}

			if (pnotify->NextEntryOffset == 0)
				break;
			pnotify = (FILE_NOTIFY_INFORMATION *)((PBYTE)pnotify + pnotify->NextEntryOffset);
		}
	}

	CloseHandle(dir);

	return true;
}