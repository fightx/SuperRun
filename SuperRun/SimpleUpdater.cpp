#include <windows.h>
#include <wininet.h>
#include <thread>
#include <vector>
#include <string>

#pragma comment(lib, "wininet.lib")

#include <codecvt>
#include <locale>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace SimpleUpdater
{
	class WininetTimeout
	{
	public:
		WininetTimeout(HINTERNET handle, int timeout_ms)
		{
			exit_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			thread = new std::thread(&WininetTimeout::wait, this, handle, timeout_ms);
		}
		~WininetTimeout()
		{
			SetEvent(exit_event);
			thread->join();
		}
	private:
		void wait(HINTERNET handle, int timeout_ms)
		{
			if (::WaitForSingleObject(exit_event, timeout_ms) == WAIT_TIMEOUT)
			{
				::InternetCloseHandle(handle);
			}
		}
	private:
		HANDLE exit_event;
		std::thread *thread;
	};

	bool HttpRequest(std::vector<char> &buffer, const char *url, const char *post = "", int timeout_ms = 1000)
	{
		bool result = false;
		//解析url
		char host[MAX_PATH + 1];
		char path[MAX_PATH + 1];

		URL_COMPONENTSA uc = { 0 };
		uc.dwStructSize = sizeof(uc);

		uc.lpszHostName = host;
		uc.dwHostNameLength = MAX_PATH;

		uc.lpszUrlPath = path;
		uc.dwUrlPathLength = MAX_PATH;

		::InternetCrackUrlA(url, 0, ICU_ESCAPE, &uc);

		HINTERNET hInet = ::InternetOpenA("Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
		if (hInet)
		{
			WininetTimeout(hInet, timeout_ms);
			HINTERNET hConn = ::InternetConnectA(hInet, host, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
			if (hConn)
			{
				DWORD flag = INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_CACHE_WRITE;
				if (uc.nScheme == INTERNET_SCHEME_HTTPS)
				{
					flag = flag | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
				}
				HINTERNET hRes = ::HttpOpenRequestA(hConn, post[0] ? "POST" : "GET", path, 0, NULL, NULL, flag, 1);
				if (hRes)
				{
					if (::HttpSendRequestA(hRes, NULL, 0, (LPVOID *)post, (DWORD)strlen(post)))
					{
						DWORD dwTotal = 0;
						DWORD dwTotalLen = sizeof(dwTotal);
						::HttpQueryInfo(hRes, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH, &dwTotal, &dwTotalLen, NULL);

						while (1)
						{
							DWORD dwLength = 0;
							DWORD dwByteRead = 0;

							if (::InternetQueryDataAvailable(hRes, &dwLength, 0, 0) && dwLength)
							{
								size_t size = buffer.size();
								buffer.resize(buffer.size() + dwLength);
								if (::InternetReadFile(hRes, &buffer[size], dwLength, &dwByteRead))
								{
									//
								}
								else
								{
									break;
								}
							}
							else
							{
								break;
							}
						}

						if (buffer.size())
						{
							buffer.push_back(0);// 补充一个字符串结束符，方便查看字符串
							result = true;
						}
					}
				}

				::InternetCloseHandle(hConn);
			}

			::InternetCloseHandle(hInet);
		}

		return result;
	}

	void CheckThread(const char *url, const char *post)
	{
		std::vector<char> buffer;
		if (HttpRequest(buffer, url, post))
		{
			const char* content = &buffer[0];
			try{
				std::wistringstream stream;
				std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;

				std::wstring json_str = conv.from_bytes(content, content + buffer.size());
				stream.str(json_str);

				boost::property_tree::wptree updater;
				boost::property_tree::json_parser::read_json(stream, updater);
			}
			catch (std::exception e)
			{
				//OutputDebugStringA(e.what());
			}
		}
	}

	void Check(const char *url, const char *post)
	{
		// 防止多次调用
		HANDLE mutex = CreateMutexA(NULL, TRUE, url);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			CloseHandle(mutex);
			return;
		}

		std::thread check_thread = std::thread(CheckThread, url, post);
		check_thread.join();
	}
}