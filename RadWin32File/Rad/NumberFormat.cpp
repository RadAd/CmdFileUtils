#include "NumberFormat.h"

#include <tchar.h>

NumberFormat::NumberFormat(LCID id)
{
    TCHAR IDigits[1024];
    TCHAR IZero[1024];
    TCHAR SGrouping[1024];
    TCHAR INegNumber[1024];
    GetLocaleInfo(id, LOCALE_IDIGITS, IDigits, 1024);
    GetLocaleInfo(id, LOCALE_ILZERO, IZero, 1024);
    GetLocaleInfo(id, LOCALE_SGROUPING, SGrouping, 1024);
    GetLocaleInfo(id, LOCALE_INEGNUMBER, INegNumber, 1024);

    GetLocaleInfo(id, LOCALE_SDECIMAL, DecimalSep, 1024);
    GetLocaleInfo(id, LOCALE_STHOUSAND, ThousandSep, 1024);

    NumDigits = _tstoi(IDigits);
    LeadingZero = _tstoi(IZero);
    Grouping = _tstoi(SGrouping);
    lpDecimalSep = DecimalSep;
    lpThousandSep = ThousandSep;
    NegativeOrder = _tstoi(INegNumber);
}

void DisplaySize(FILE* out, ULARGE_INTEGER size, const NUMBERFMT* nf)
{
    TCHAR NumberIn[1024];
    _stprintf_s(NumberIn, TEXT("%d"), (int) size.QuadPart);

    TCHAR Number[1024];
    /*int Length =*/ GetNumberFormat(LOCALE_USER_DEFAULT, 0, NumberIn, nf, Number, 1024);
    //DisplayPadding(o, 15 - Length);

    _ftprintf(out, Number);
}
