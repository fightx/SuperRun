#pragma once

#include "element.h"
#include "i18n.h"

//内置搜索目录
scanner_directory builtin_directory[] = {
	{ LR"(%APPDATA%\Microsoft\Windows\Start Menu\Programs)", 3 },
	{ LR"(%PROGRAMDATA%\Microsoft\Windows\Start Menu\Programs)", 3 },
	{ LR"(%APPDATA%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar)", 3 },
	{ LR"(%USERPROFILE%\Desktop)", 3 },
};

//内置命令
typedef void (*builtin_function) (HWND);
typedef bool (*function_isvalid) (HWND);
struct builtin_command
{
	const std::wstring command;
	builtin_function function;
	function_isvalid check;
};

struct builtin_command_name
{
	const std::wstring name;
	const std::wstring command;
};

void about(HWND hwnd)
{
	TipsBox(i18n::GetString(L"about_info").c_str());
}
void exit(HWND hwnd)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0);
}

bool is_autorun(HWND hwnd)
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
	{
		wchar_t buffer[MAX_PATH];
		DWORD dwLength = MAX_PATH;
		if (RegQueryValueEx(hKey, app_name, NULL, NULL, (LPBYTE)buffer, &dwLength) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);

			wchar_t path[MAX_PATH];
			GetPrettyPath(path);

			if (wcsicmp(path, buffer) == 0)
			{
				return true;
			}
		}
		else
		{
			RegCloseKey(hKey);
		}
	}

	return false;
}

bool is_not_autorun(HWND hwnd)
{
	return !is_autorun(hwnd);
}

void switch_autorun(HWND hwnd)
{
	HKEY hKey;
	RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &hKey);
	if (is_not_autorun(hwnd))
	{
		TCHAR path[MAX_PATH];
		GetPrettyPath(path);
		if (_tcsstr(path, _T("Temp")) != NULL)
		{
			if (TipsBox(i18n::GetString(L"autorun_tip").c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDNO) return;
		}
		RegSetValueEx(hKey, app_name, 0, REG_SZ, (LPBYTE)path, _tcslen(path)*sizeof(TCHAR));
	}
	else
	{
		RegDeleteValue(hKey, app_name);
	}
	RegCloseKey(hKey);
}


void append(HWND hwnd)
{
	wchar_t dir[MAX_PATH];
	if (BrowseFolder(dir, L"", NULL))
	{
		SendMessage(hwnd, WM_USER_ADDDIR, (WPARAM)dir, 0);
	}
}

builtin_command builtin_command_list[] = {
	{ L"about", &about },
	{ L"exit", &exit },
	{ L"append", &append },
	{ L"autorun_enable", &switch_autorun, &is_not_autorun },
	{ L"autorun_disable", &switch_autorun, &is_autorun },
};


std::list<builtin_command_name> builtin_command_name_list;
const std::list<builtin_command_name> &get_builtin_command_list(HWND hwnd)
{
	builtin_command_name_list.clear();
	for (auto &command : builtin_command_list)
	{
		if (command.check)
		{
			if (command.check(hwnd))
			{
				builtin_command_name_list.push_back({ i18n::GetString(command.command), command.command });
			}
		}
		else
		{
			builtin_command_name_list.push_back({ i18n::GetString(command.command), command.command });
		}
	}

	builtin_command_name_list.sort([](const builtin_command_name & a, const builtin_command_name & b) {
		return StrCmpLogicalW(a.command.c_str(), b.command.c_str())<0;
	});

	return builtin_command_name_list;
}

