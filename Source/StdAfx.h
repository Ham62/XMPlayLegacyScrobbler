// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__578DBC06_140B_4EEC_90FE_466990407FA4__INCLUDED_)
#define AFX_STDAFX_H__578DBC06_140B_4EEC_90FE_466990407FA4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma comment(lib,"ws2_32.lib")  //Winsock Library
#pragma comment(lib,"lib\\ssleay32.lib") //OpenSSL
#pragma comment(lib,"lib\\libeay32.lib") //OpenSSL

#define STRLEN(_s) ((sizeof(_s)/sizeof(_s[0]))-1)

#include <cstdio>

#define _WIN32_WINNT 0x0400 // Target WinNT 4+

#include <stdio.h>

#include <windows.h>
#include <Wincrypt.h>

#include <openssl\ssl.h>

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__578DBC06_140B_4EEC_90FE_466990407FA4__INCLUDED_)
