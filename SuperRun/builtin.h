#pragma once

#include "element.h"

//内置搜索目录
scanner_directory builtin_directory[] = {
	{ LR"(%APPDATA%\Microsoft\Windows\Start Menu\Programs)", 3 },
	{ LR"(%PROGRAMDATA%\Microsoft\Windows\Start Menu\Programs)", 3 },
	{ LR"(%APPDATA%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar)", 3 },
	{ LR"(%USERPROFILE%\Desktop)", 3 },
};

//内置命令
typedef void (*builtin_function) (HWND);
struct builtin_command
{
	const std::wstring name;
	const std::wstring command;
	builtin_function function;
};

void about(HWND hwnd)
{
	//ShellExecute(NULL, L"open", L"http://www.shuax.com", L"", L"", SW_SHOWNORMAL);
	MessageBox(NULL, L"欢迎使用SuperRun，目前正在开发中。", L"SuperRun", 0);
}
void exit(HWND hwnd)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0);
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
	{ L"关于", L"about", &about },
	{ L"退出", L"exit", &exit },
	{ L"添加目录", L"append", &append },
};