#pragma once

#include <shlwapi.h>

#include <boost/filesystem.hpp>
#include <map>

typedef std::map<std::wstring, std::wstring> special_name_map;

//展开环境路径比如 %windir%
std::wstring ExpandEnvironmentPath(const std::wstring& path)
{
	std::vector<wchar_t> buffer(MAX_PATH);
	size_t expandedLength = ::ExpandEnvironmentStrings(path.c_str(), &buffer[0], buffer.size());
	if (expandedLength > buffer.size())
	{
		buffer.resize(expandedLength);
		expandedLength = ::ExpandEnvironmentStrings(path.c_str(), &buffer[0], buffer.size());
	}
	return std::wstring(&buffer[0], 0, expandedLength);
}

//解析ini中的特殊字段
void ParseIniSpecialName(const std::wstring& path, special_name_map &special_name)
{
	std::vector<wchar_t> buffer(4 * 1024);
	::GetPrivateProfileSection(L"LocalizedFileNames", &buffer[0], buffer.size(), path.c_str());
	wchar_t *line = &buffer[0];
	while (line && *line)
	{
		size_t len = wcslen(line);

		std::wstring line_ = std::wstring(line, 0, len);
		std::size_t p1 = line_.find(L"=@");
		if (p1 != std::wstring::npos)
		{
			std::wstring value = line_.substr(p1 + 1);
			
			std::vector<wchar_t> buffer(MAX_PATH);

			int buflen = buffer.size();

			SHLoadIndirectString(value.c_str(), &buffer[0], buflen, NULL);

			special_name[line_.substr(0, p1)] = std::wstring(&buffer[0], 0, buflen);
		}

		line += len + 1;
	}
}

// 读取目录下Desktop.ini的特殊值
void GetSpecialName(const boost::filesystem::wpath &directory, special_name_map &special_name)
{
	//清除上次的数据
	special_name.clear();
	boost::filesystem::wpath desktopini = directory / L"Desktop.ini";

	boost::system::error_code ec;
	if (boost::filesystem::is_regular_file(desktopini, ec))
	{
		if (::GetFileAttributes(desktopini.c_str()) & FILE_ATTRIBUTE_SYSTEM)
		{
			ParseIniSpecialName(desktopini.wstring(), special_name);
		}
	}
}

#include <shlobj.h>
#include <objbase.h>

bool BrowseFolder(wchar_t *path, const wchar_t *title, HWND hwnd)
{
	bool result = false;
	IFileOpenDialog *pFileOpen;

	// Create the FileOpenDialog object.
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
								  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

	if (SUCCEEDED(hr))
	{
		DWORD dwOptions;
		if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions)))
		{
			pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
		}

		pFileOpen->SetTitle(title);

		// Show the Open dialog box.
		hr = pFileOpen->Show(hwnd);

		// Get the file name from the dialog box.
		if (SUCCEEDED(hr))
		{
			IShellItem *pItem;
			hr = pFileOpen->GetResult(&pItem);
			if (SUCCEEDED(hr))
			{
				PWSTR pszFilePath;
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

				// Display the file name to the user.
				if (SUCCEEDED(hr))
				{
					wcsncpy_s(path, MAX_PATH, pszFilePath, MAX_PATH);
					result = true;
					CoTaskMemFree(pszFilePath);
				}
				pItem->Release();
			}
		}
		pFileOpen->Release();
	}
	else
	{
		BROWSEINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.hwndOwner = hwnd;
		bi.lpszTitle = title;

		bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
//        bi.lpfn = BrowserCallbackProc;
//        bi.lParam = (LPARAM)path;//初始化位置

		if( SHGetPathFromIDList(SHBrowseForFolder(&bi), path) )
		{
			result = true;
		}
	}

	return result;
}
#include <shlobj.h>

HRESULT ResolveIt(const wchar_t *lpszLinkFile, wchar_t *lpszPath, int iPathBufferSize)
{
	CoInitialize(NULL);

	IShellLinkW* psl;
	HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
	if (SUCCEEDED(hres))
	{
		IPersistFile* ppf;
		hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
		if (SUCCEEDED(hres))
		{
			hres = ppf->Load(lpszLinkFile, STGM_READ);
			if (SUCCEEDED(hres))
			{
				hres = psl->Resolve(NULL, SLR_NO_UI | SLR_ANY_MATCH | SLR_NOUPDATE | SLR_NOSEARCH | SLR_NOTRACK | SLR_NOLINKINFO);
				if (SUCCEEDED(hres))
				{
					WIN32_FIND_DATA wfd;
					hres = psl->GetPath(lpszPath, iPathBufferSize, (WIN32_FIND_DATAW*)&wfd, SLGP_RAWPATH);
				}
			}
			ppf->Release();
		}
		psl->Release();
	}
	CoUninitialize();
	return hres;
}

#pragma comment(lib, "version.lib")

//
//	Get the specified file-version information string from a file
//
//	szItem	- version item string, e.g:
//		"FileDescription", "FileVersion", "InternalName",
//		"ProductName", "ProductVersion", etc  (see MSDN for others)
//
TCHAR *GetVersionString(const TCHAR *szFileName, const TCHAR *szValue, TCHAR *szBuffer, ULONG nLength)
{
	DWORD  len;
	PVOID  ver;
	DWORD  *codepage;
	TCHAR   fmt[0x40];
	PVOID  ptr = 0;
	BOOL   result = FALSE;

	szBuffer[0] = '\0';

	len = GetFileVersionInfoSize(szFileName, 0);

	if (len == 0 || (ver = malloc(len)) == 0)
		return NULL;

	if (GetFileVersionInfo(szFileName, 0, len, ver))
	{
		//从版本资源中获取信息。调用这个函数前，必须先用GetFileVersionInfo函数获取版本资源信息。
		// 这个函数会检查资源信息，并将需要的数据复制到一个缓冲区里
		//"\" 获取文件的VS_FIXEDFILEINFO结构
		//"\VarFileInfo\Translation" 获取文件的翻译表
		//"\StringFileInfo\...." 获取文件的字串信息。参考注解
		if (VerQueryValue(ver, _T("\\VarFileInfo\\Translation"), (void **)&codepage, (PUINT)&len))
		{
			wsprintf(fmt, _T("\\StringFileInfo\\%04x%04x\\%s"), (*codepage) & 0xFFFF,
				(*codepage) >> 16, szValue);

			if (VerQueryValue(ver, fmt, &ptr, (PUINT)&len))
			{
				lstrcpyn(szBuffer, (TCHAR*)ptr, std::min(nLength, len));
				result = TRUE;
			}
		}
	}

	free(ver);
	return result ? szBuffer : NULL;
}

std::wstring GetDefaultLanguage()
{
	wchar_t language[MAX_PATH];
	if (!GetLocaleInfo(GetUserDefaultUILanguage(), LOCALE_SISO639LANGNAME, language, MAX_PATH))
	{
		return L"zh-CN";
	}
	wchar_t country[MAX_PATH];
	if (!GetLocaleInfo(GetUserDefaultUILanguage(), LOCALE_SISO3166CTRYNAME, country, MAX_PATH))
	{
		return std::wstring(language);
	}

	return std::wstring(language) + L"-" + country;
}

boost::filesystem::path GetAppPath()
{
	wchar_t app_path[MAX_PATH];
	::GetModuleFileName(NULL, app_path, MAX_PATH);

	return boost::filesystem::wpath(app_path).parent_path();
}

void GetPrettyPath(TCHAR *path)
{
	TCHAR temp[MAX_PATH];
	GetModuleFileName(NULL, temp, MAX_PATH);
	PathCanonicalize(path, temp);
	PathQuoteSpaces(path);
}