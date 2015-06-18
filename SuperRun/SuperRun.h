#pragma once

#pragma warning(disable:4267)
#pragma warning(disable:4244)

#define NOMINMAX

// Windows ͷ�ļ�: 
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <objbase.h>

//#include <vld.h>

// C ����ʱͷ�ļ�
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#define _WTL_NO_CSTRING
#include <atlbase.h>
#include <atlstr.h>
#include <atlapp.h>
#include <atlwin.h>

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>
#include <atlmisc.h>
#include <atlcrack.h>

#pragma comment(lib, "comctl32.Lib")

#include <boost/thread/thread.hpp>

#define WM_USER_SHOW (WM_USER + 1)
#define WM_USER_HIDE (WM_USER + 2)
#define WM_USER_KEYUP (WM_USER + 3)
#define WM_USER_ADDDIR (WM_USER + 4)


#define KEY_PRESSED 0x8000

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")