#include "SuperRun.h"

boost::property_tree::wptree settings;

enum COMMAND_TYPE
{
	COMMAND_GENERAL,
	COMMAND_BUILTIN,
	COMMAND_CRYPTO,
};

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	static bool repeat = false;
	if (nCode == HC_ACTION)
	{
		if (wParam == WM_KEYDOWN)
		{
			KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT*)lParam;
			if (kbd->vkCode == 'R' && (GetAsyncKeyState(VK_LWIN) & KEY_PRESSED || GetAsyncKeyState(VK_RWIN) & KEY_PRESSED))
			{
				if (!repeat)
				{
					if (GetForegroundWindow() != hwnd)
					{
						::PostMessage(hwnd, WM_USER_SHOW, 0, 0);
					}
					else
					{
						::PostMessage(hwnd, WM_USER_HIDE, 0, 0);
					}
				}
				repeat = true;
				return 1;
			}
			if (kbd->vkCode == VK_UP || kbd->vkCode == VK_DOWN || kbd->vkCode == VK_ESCAPE || kbd->vkCode == VK_RETURN || kbd->vkCode==VK_TAB)
			{
				if (GetForegroundWindow() == hwnd)
				{
					::PostMessage(hwnd, WM_USER_KEYUP, kbd->vkCode, 0);
					return 1;
				}
			}
		}
		else
		{
			repeat = false;
		}
	}

	return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

class SuperRunUI :
	public ATL::CWindowImpl<SuperRunUI>
{
public:
	DECLARE_WND_CLASS(app_name)

	BEGIN_MSG_MAP(SuperRunUI)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_TIMER(OnTimer)
		MSG_WM_ACTIVATE(OnActivate)
		MSG_WM_COMMAND(OnCommand)

		MESSAGE_HANDLER(WM_NCLBUTTONDOWN, OnNcLButtonDown)
		
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

		MESSAGE_HANDLER(WM_USER_ACTIVE, OnUserActive)
		MESSAGE_HANDLER(WM_USER_SHOW, OnUserShow)
		MESSAGE_HANDLER(WM_USER_HIDE, OnUserHide)
		MESSAGE_HANDLER(WM_USER_KEYUP, OnUserKeyUp)
		MESSAGE_HANDLER(WM_USER_ADDDIR, OnUserAddDir)
	END_MSG_MAP()

	void ScanWatchThread(const scanner_directory &scanner)
	{
		std::wstring fullpath = ExpandEnvironmentPath(scanner.path);
		if (PathIsRelative(fullpath.c_str()))
		{
			wchar_t absolute[MAX_PATH] = { 0 };
			PathCombine(absolute, GetAppPath().wstring().c_str(), fullpath.c_str());
			fullpath = absolute;
		}
		HANDLE ready_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		boost::thread t = boost::thread(WatchDirectory, &run_list, fullpath, scanner.level, exit_event, ready_event);
		::WaitForSingleObject(ready_event, INFINITE);

		ScanDirectory(&run_list, fullpath, scanner.level, exit_event);

		t.join();
	}

	void InitFont()
	{
		LOGFONT lf = { 0 };
		NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, false);
		lf = ncm.lfMessageFont;

		ATLVERIFY(font.CreateFontIndirect(&lf));
	}

	LRESULT OnUserActive(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return OnUserShow(uMsg, wParam, lParam, bHandled);
	}

	LRESULT OnUserShow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		ShowWindow(SW_SHOW);
		SetForegroundWindow(m_hWnd);

		Sleep(100);

		POINT pt;
		GetCursorPos(&pt);

		RECT rect;
		GetWindowRect(&rect);

		SetCursorPos(rect.left + 10, rect.top + 10);

		INPUT input[1];
		memset(input, 0, sizeof(input));

		input[0].type = INPUT_MOUSE;

		input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		SendInput(1, input, sizeof(INPUT));

		input[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
		SendInput(1, input, sizeof(INPUT));

		SetCursorPos(pt.x, pt.y);

		ShowWindowUI();
		return 0;
	}

	LRESULT OnUserHide(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		HideWindow();
		return 0;
	}

	LRESULT OnUserKeyUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		switch (wParam)
		{
		case VK_UP:
			if (GetFocus() == edit)
			{
				if (list.GetCurSel() <= 0)
				{
					list.SetCurSel(list.GetCount() - 1);
				}
				else
				{
					list.SetCurSel(list.GetCurSel() - 1);
				}
			}
			break;
		case VK_DOWN:
			if (GetFocus() == edit)
			{
				if (list.GetCurSel() >= list.GetCount() - 1)
				{
					list.SetCurSel(0);
				}
				else
				{
					list.SetCurSel(list.GetCurSel() + 1);
				}
			}
			break;
		case VK_ESCAPE:
			if (edit.GetWindowTextLengthW())
			{
				//编辑框不为空
				edit.SetWindowTextW(L"");
			}
			else
			{
				HideWindow();
			}
			break;

		case VK_RETURN:
			RunList(list.GetCurSel() != LB_ERR);
			break;
		}
		return 1;
	}

	LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		is_show = false;
		hwnd = m_hWnd;

		WM_USER_ACTIVE = ::RegisterWindowMessage(unique_guid);

		run_list.Load(index_config);

		exit_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);

		for (auto scanner : builtin_directory)
		{
			group.add_thread(new boost::thread(&SuperRunUI::ScanWatchThread, this, scanner));
		}

		try{
			for (auto scan : settings.get_child(L"scan_directory"))
			{
				scanner_directory scanner = {};

				size_t index = 0;
				for (auto item : scan.second)
				{
					switch (index)
					{
					case 0:
						scanner.path = item.second.get_value<std::wstring>();
						break;
					case 1:
						scanner.level = item.second.get_value<uint32_t>();
						break;
					}
					index++;
				}
				group.add_thread(new boost::thread(&SuperRunUI::ScanWatchThread, this, scanner));
			}
		}
		catch (...)
		{
			boost::property_tree::wptree scan_directory;
			settings.put_child(L"scan_directory", scan_directory);
		}

		try{
			for (auto scan : settings.get_child(L"hotkey"))
			{
			}
		}
		catch (...)
		{
			boost::property_tree::wptree hotkey;
			boost::property_tree::wptree key;

			key.put(L"", L"Left Windows+R");
			hotkey.push_back(std::make_pair(L"", key));

			settings.put_child(L"hotkey", hotkey);
		}
		
		InitFont();
		CenterWindow();

		RECT rect;
		GetWindowRect(&rect);
		rect.bottom -= rect.top;
		rect.top -= rect.top;
		SetWindowPos(NULL, &rect, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

		RECT pos = { 0, 0, 390, 18 };
		edit.Create(m_hWnd, pos, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL);
		edit.SetFont(font);

		pos = { 0, 20, 395, 400-30 };
		list.Create(m_hWnd, pos, L"", WS_CHILD | WS_VSCROLL | WS_VISIBLE | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY);
		list.SetFont(font);
		list.SetItemHeight(NULL, 24);

		keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, instance, 0);

		SetTimer(0, 200);

		if (settings.get<bool>(L"first_run", true))
		{
			settings.put(L"first_run", false);
			ShowBalloon();
		}

		return 0;
	}

	void ShowBalloon()
	{
		memset(&nid, 0, sizeof(nid));
		nid.cbSize = sizeof(NOTIFYICONDATA);
		nid.uID = 100;
		nid.hWnd = hwnd;
		nid.dwInfoFlags = NIIF_INFO;
		nid.uFlags = NIF_ICON | NIF_TIP | NIF_INFO;
		nid.hIcon = LoadIcon(instance, L"APPICON");
		wcscpy_s(nid.szInfoTitle, _countof(nid.szInfoTitle), i18n::GetString(L"tray_title").c_str());
		wcscpy_s(nid.szInfo, _countof(nid.szInfo), i18n::GetString(L"tray_info").c_str());
		wcscpy_s(nid.szTip, _countof(nid.szTip), i18n::GetString(L"name").c_str());
		Shell_NotifyIcon(NIM_ADD, &nid);
		Shell_NotifyIcon(NIM_DELETE, &nid);
	}

	void OnTimer(UINT_PTR nIDEvent)
	{
		switch (nIDEvent)
		{
		case 0:
			if (IsWindowVisible() && current_type==COMMAND_GENERAL && run_list.hasChanged())
			{
				UpdateList();
			}
			break;
		}
		return;
	}

	void OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther)
	{
		if (nState == WA_INACTIVE)
		{
			HideWindow();
		}
	}

	LRESULT OnNcLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		return 1;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		UnhookWindowsHook(WH_KEYBOARD_LL, LowLevelKeyboardProc);
		SetEvent(exit_event);
		group.join_all();
		run_list.Save(index_config);
		::PostQuitMessage(0);
		return 0;
	}
	
	template <class MatchList, class Function>
	void ShowEdit(MatchList &match_list, Function f)
	{
		rank_list_command.clear();

		for (auto &run : match_list)
		{
			f(run);
		}
		
		rank_list_command.sort([](const rank_list & a, const rank_list & b) {
			return a.rank>b.rank;
		});

		list.ResetContent();
		for (const rank_list &run : rank_list_command)
		{
			int index = list.GetCount();

			list.AddString(run.name.c_str());
			list.SetItemDataPtr(index, (void*)run.command.c_str());
		}

		list.SetCurSel(0);
	}

	void OnEdit(const std::wstring &search)
	{
		if (search[0] == '!')
		{
			current_type = COMMAND_BUILTIN;
			
			ShowEdit(get_builtin_command_list(hwnd), [search, this](const builtin_command_name &run) {
				if (double rank = ranker(search.c_str() + 1, run.name))
				{
					rank_list_command.push_back({ run.name, run.command, rank });
				}
			});
		}
		else
		{
			current_type = COMMAND_GENERAL;

			uint32_t total = run_list.get_total();
			ShowEdit(run_list.get(), [search, this, total](const super_run &run) {
				if (double rank = ranker(search, run.description))
				{
					rank += 1.0*run.count/total;
					rank_list_command.push_back({ run.name, run.fullpath, rank });
				}
			});
		}
	}

	void OnCommand(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		if (wndCtl == edit)
		{
			if (uNotifyCode == EN_CHANGE && is_show)
			{
				//文本框有改变
				UpdateList();
			}
		}
		else if (wndCtl == list )
		{
			if (uNotifyCode == LBN_SELCHANGE && is_show)
			{
				//鼠标点击了一项列表
				RunList(true);
			}
		}
	}

	void UpdateList()
	{
		CString search;
		edit.GetWindowText(search);
		std::wstring lower_search = search;
		boost::algorithm::to_lower(lower_search);
		OnEdit(remove_spaces(lower_search));
	}

	void RunListCommand(bool have_choice, std::wstring command)
	{
		switch (current_type)
		{
		case COMMAND_GENERAL:
			RunGeneral(have_choice, command.c_str());
			break;
		case COMMAND_BUILTIN:
			RunBuiltin(have_choice, command.c_str());
			break;
		}
	}
	void RunList(bool have_choice)
	{
		std::wstring command;

		if (have_choice)
		{
			//列表中有选择
			int index = list.GetCurSel();
			const void * data = list.GetItemDataPtr(index);
			command = std::wstring((const wchar_t *)data);
		}
		else
		{
			//列表为空
			CString text;
			edit.GetWindowText(text);
			command = text;
		}
		
		HideWindow();

		RunListCommand(have_choice, command);
		//group.add_thread(new boost::thread(&SuperRunUI::RunListCommand, this, have_choice, command));
	}

	void RunGeneral(bool have_choice, const wchar_t * command)
	{
		//判断是否是注册表
		if (!have_choice && boost::istarts_with(command, L"HKEY_"))
		{
			HKEY hKey;
			if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit"), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
			{
				RegSetValueEx(hKey, L"LastKey", 0, REG_SZ, (LPBYTE)command, _tcslen(command)*sizeof(TCHAR));
				RegCloseKey(hKey);
			}
			command = L"regedit";
		}

		SHELLEXECUTEINFO ShExecInfo = { 0 };
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.lpFile = command;
		ShExecInfo.nShow = SW_SHOW;

		std::wstring argv = command;
		std::wstring lpFile;
		std::wstring lpParameters;
		if (!have_choice)
		{
			std::size_t space = argv.find(L" ");
			if (space != std::wstring::npos)
			{
				lpFile = argv.substr(0, space);
				lpParameters = argv.substr(space + 1);
				ShExecInfo.lpFile = lpFile.c_str();
				ShExecInfo.lpParameters = lpParameters.c_str();
			}
		}

		if (ShellExecuteEx(&ShExecInfo))
		{
			if (have_choice)
			{
				run_list.AddCount(command);
			}
		}

	}

	void RunBuiltin(bool have_choice, const wchar_t * command)
	{
		if (!have_choice) return;
		for (auto &builtin_command : builtin_command_list)
		{
			if (builtin_command.command == command)
			{
				builtin_command.function(hwnd);
				return;
			}
		}
	}


	LRESULT OnUserAddDir(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		std::wstring directory = std::wstring((const wchar_t *)wParam);
		boost::filesystem::wpath path(directory);

		boost::system::error_code ec;
		if (!boost::filesystem::is_directory(path, ec))
		{
			return 0;
		}

		if (path.wstring().find(GetAppPath().wstring()) != std::wstring::npos)
		{
			wchar_t relative_path[MAX_PATH];
			PathRelativePathTo(relative_path, GetAppPath().wstring().c_str(), FILE_ATTRIBUTE_DIRECTORY, directory.c_str(), FILE_ATTRIBUTE_DIRECTORY);
			directory = relative_path;
		}

		boost::property_tree::wptree scan_directory = settings.get_child(L"scan_directory");

		bool alreay_add = false;
		for (auto scan : scan_directory)
		{
			size_t index = 0;
			for (auto item : scan.second)
			{
				switch (index)
				{
				case 0:
					if (item.second.get_value<std::wstring>() == directory)
					{
						alreay_add = true;
						break;
					}
					break;
				}
				index++;
			}
		}
		if (!alreay_add)
		{
			boost::property_tree::wptree item;
			boost::property_tree::wptree child1, child2;

			child1.put(L"", directory);
			child2.put(L"", 3);
			item.push_back(std::make_pair(L"", child1));
			item.push_back(std::make_pair(L"", child2));
			
			scan_directory.push_back(std::make_pair(L"", item));
			settings.put_child(L"scan_directory", scan_directory);

			scanner_directory scanner = { directory, 3 };
			group.add_thread(new boost::thread(&SuperRunUI::ScanWatchThread, this, scanner));
		}
		return 0;
	}

	void HideWindow()
	{
		is_show = false;
		list.ResetContent();
		edit.SetWindowTextW(L"");
		ShowWindow(SW_HIDE);
	}

	void ShowWindowUI()
	{
		is_show = true;
		edit.SetFocus();
		UpdateList();
	}
protected:
	CEdit edit;
	CFont font;
	CListBox list;
	boost::thread_group group;
	HANDLE exit_event;

	int WM_USER_ACTIVE;

	super_run_list run_list;

	int current_type;
	bool is_show;

	std::list<rank_list> rank_list_command;

	NOTIFYICONDATA nid;
};


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	i18n::Init();

	//检查操作系统
	BOOL bIsWow64 = FALSE;
	IsWow64Process(GetCurrentProcess(), &bIsWow64);
	if (bIsWow64)
	{
		return TipsBox(i18n::GetString(L"platform_error").c_str(), MB_ICONWARNING);
	}

	// 检查单例，并且唤出界面
	int WM_USER_ACTIVE = ::RegisterWindowMessage(unique_guid);
	HANDLE mutex = CreateMutex(NULL, TRUE, unique_guid);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		PostMessage(HWND_BROADCAST, WM_USER_ACTIVE, 3, 0);
		CloseHandle(mutex);
		return 0;
	}

	SimpleUpdater::Check("http://tools.shuax.com/getchrome", R"(data={"branch":"Stable","arch":"x86"})");

	InitCommonControls();

	LoadFromFile(settings, GetDataPath(settings_config).c_str());

	hotkey_init();

	instance = hInstance;

	SuperRunUI UI;
	if (!UI.Create(NULL, CRect(0, 0, 400, 400), i18n::GetString(L"title").c_str(), WS_CAPTION, WS_EX_COMPOSITED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST))
	{
		return 0;
	}
	//UI.ShowWindow(nCmdShow);

	WTL::CMessageLoop msgLoop;
	int ret = msgLoop.Run();

	SaveToFile(settings, GetDataPath(settings_config).c_str());
	return ret;
}
