#pragma once

#include <map>

std::map<std::wstring, short> key_list;

void hotkey_init()
{
	for (short key = 0; key<256; key++)
	{
		wchar_t szKey[MAX_PATH];
		int scan_code = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
		int extended = (key == VK_LWIN) || (key == VK_RWIN);
		if (GetKeyNameText(scan_code << 16 | (extended << 24), szKey, MAX_PATH))
		{
			key_list[szKey] = scan_code;
		}
	}

}