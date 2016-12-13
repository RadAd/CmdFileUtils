#ifndef NUMBERFORMAT_H
#define NUMBERFORMAT_H

#include <Windows.h>
#include <stdio.h>

class NumberFormat : public NUMBERFMT
{
public:
    NumberFormat(LCID id);

private:
    TCHAR DecimalSep[1024];
    TCHAR ThousandSep[1024];
};

void DisplaySize(FILE* out, ULARGE_INTEGER size, const NUMBERFMT* nf);

#endif
