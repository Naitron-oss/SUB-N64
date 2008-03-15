#ifndef WINLNXDEFS_H
#define WINLNXDEFS_H

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int INT;
typedef long long LONGLONG;

typedef int __int32;

typedef void* HINSTANCE;
typedef int PROPSHEETHEADER;
typedef int PROPSHEETPAGE;
typedef int HWND;

#define FALSE false
#define TRUE true
#define __stdcall
#define __declspec(dllexport)
#define _cdecl
#define WINAPI

typedef union _LARGE_INTEGER
{
   struct
     {
	DWORD LowPart;
	INT HighPart;
     } s;
   struct
     {
	DWORD LowPart;
	INT HighPart;
     } u;
   LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

#define HIWORD(a) ((unsigned int)(a) >> 16)
#define LOWORD(a) ((a) & 0xFFFF)

#endif // WINLNXDEFS_H
