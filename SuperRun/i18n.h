#pragma once

#include <string>
#include "serialize.h"
#include "util.h"
//#include <boost\locale.hpp>

namespace i18n
{
	boost::property_tree::wptree language;

	void Init()
	{
		//加载当前系统语言包
		std::wstring lang = GetDefaultLanguage() + L".json";
		boost::filesystem::wpath lang_path = GetLangPath(lang.c_str());
		if (!LoadFromFile(language, lang_path.c_str()))
		{
			//加载文件不成功，加载资源中的内置语言包
			HRSRC res = FindResource(NULL, L"LANG", L"JSON");
			if (res)
			{
				HGLOBAL header = LoadResource(NULL, res);
				if (header)
				{
					const char *data = (const char*)LockResource(header);
					DWORD size = SizeofResource(NULL, res);
					if (data)
					{
						try{
							std::wistringstream stream;
							std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;

							std::wstring json_str = conv.from_bytes(data, data + size);
							stream.str(json_str);

							boost::property_tree::json_parser::read_json(stream, language);
						}
						catch (std::exception e)
						{
							OutputDebugStringA(e.what());
						}

						UnlockResource(header);
					}
				}
				FreeResource(header);
			}
		}
	}
	std::wstring GetString(std::wstring name)
	{
		return language.get<std::wstring>(name, name);
	}
}

int TipsBox(const wchar_t *tips, UINT type = MB_OK)
{
	return MessageBoxW(NULL, tips, i18n::GetString(L"name").c_str(), type);
}
