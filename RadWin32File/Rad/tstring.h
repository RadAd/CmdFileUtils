#ifndef TSTRING_H
#define TSTRING_H

#include <tchar.h>

#ifdef _UNICODE
#define tmain wmain
#define tcout wcout
#define tcin wcin
#define tcerr wcerr
#define tstring wstring
#define tostream wostream
#define tostringstream wostringstream
#else
#define tmain main
#define tcout cout
#define tcin cin
#define tcerr cerr
#define tstring string
#define tostream ostream
#define tostringstream ostringstream
#endif

#endif
