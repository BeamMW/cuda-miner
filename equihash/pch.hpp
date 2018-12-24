
#pragma once

#ifdef _WIN32
// fixes
//------------------------------------
#define	_CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
//------------------------------------

#define	_WIN32_WINNT 0x0601

#include <SDKDDKVer.h>

#include <WinSock2.h>
#include <windows.h>
#include <WinInet.h>	
#include <shlobj.h>
#include <tchar.h>
#else
typedef unsigned int DWORD;
typedef unsigned long ULONG;
#define	STDMETHODCALLTYPE
#endif

#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <cstring>
#include <set>
#include <vector>
#include <unordered_map>

#ifdef _WIN32
#pragma comment (lib, "Ws2_32.lib")
#endif
