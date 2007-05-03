// svdrand.h -- Random Numbers, CLinearTimeAbsolute, and CLinearTimeDelta
// modules
//
// $Id: svdrand.h,v 1.1 2000-04-11 07:14:48 sdennis Exp $
//
// Random Numbers based on algorithms presented in "Numerical Recipes in C",
// Cambridge Press, 1992.
// 
// Date/Time code based on algorithms presented in "Calendrical Calculations",
// Cambridge Press, 1998.
//
// RandomLong() and do_convtime() were derived from existing game server code.
//
// MUX 2.0
// Copyright (C) 1998 through 2000 Solid Vertical Domains, Ltd. All
// rights not explicitly given are reserved. Permission is given to
// use this code for building and hosting text-based game servers.
// Permission is given to use this code for other non-commercial
// purposes. To use this code for commercial purposes other than
// building/hosting text-based game servers, contact the author at
// Stephen Dennis <sdennis@svdltd.com> for another license.
//
#ifndef SVDRAND_H
#define SVDRAND_H

void SeedRandomNumberGenerator(void);
double RandomFloat(double flLow, double flHigh);
long RandomLong(long lLow, long lHigh);

typedef struct
{
             short iYear;       // 1900 would be stored as 1900.
    unsigned short iMonth;      // January is 1. December is 12.
    unsigned short iDayOfWeek;  // 0 is Sunday, 1 is Monday, etc.
    unsigned short iDayOfMonth; // Day of Month, 1..31
    unsigned short iDayOfYear;  // January 1st is 1, etc.
    unsigned short iHour;
    unsigned short iMinute;
    unsigned short iSecond;
    unsigned short iMillisecond; // Milliseconds less than a second.
    unsigned short iMicrosecond; // Microseconds less than a Millisecond.
    unsigned short iNanosecond;  // Nanoseconds less than a Microsecond.
} FIELDEDTIME;

class CLinearTimeAbsolute
{
    friend class CLinearTimeDelta;
    friend int operator<(const CLinearTimeAbsolute& lta, const CLinearTimeAbsolute& ltb);
    friend int operator>(const CLinearTimeAbsolute& lta, const CLinearTimeAbsolute& ltb);
    friend int operator==(const CLinearTimeAbsolute& lta, const CLinearTimeAbsolute& ltb);
    friend int operator<=(const CLinearTimeAbsolute& lta, const CLinearTimeAbsolute& ltb);
    friend CLinearTimeAbsolute operator+(const CLinearTimeAbsolute& lta, const CLinearTimeDelta& ltd); 
    friend CLinearTimeAbsolute operator-(const CLinearTimeAbsolute& lta, const CLinearTimeDelta& ltd); 
    friend CLinearTimeDelta operator-(const CLinearTimeAbsolute& lta, const CLinearTimeAbsolute& ltb);

private:
    INT64  m_tAbsolute;
    static int m_nCount;
    static char m_Buffer[204];

public:
    //CLinearTimeAbsolute(int tInitial);
    CLinearTimeAbsolute(void);
    CLinearTimeAbsolute(const CLinearTimeAbsolute& ltaOrigin, const CLinearTimeDelta& ltdOffset);
    CLinearTimeAbsolute(const CLinearTimeAbsolute& lta);
    void operator=(const CLinearTimeAbsolute& lta);
    void operator+=(const CLinearTimeDelta& ltdOffset);
    void operator-=(const CLinearTimeDelta& ltdOffset);

    void GetUTC(void);
    void GetLocal(void);

    void  ReturnUniqueString(char *buffer);
    char *ReturnDateString(void);
    BOOL  ReturnFields(FIELDEDTIME *arg_tStruct);
    INT64 ReturnSeconds(void);
    char *ReturnSecondsString(void);

    void SetSeconds(INT64 arg_tSeconds);
    void SetSecondsString(char *arg_szSeconds);
    BOOL SetFields(FIELDEDTIME *arg_tStruct);
    BOOL SetString(const char *arg_tBuffer);
};

BOOL FieldedTimeToLinearTime(FIELDEDTIME *ft, INT64 *plt);
BOOL LinearTimeToFieldedTime(INT64 lt, FIELDEDTIME *ft);

class CLinearTimeDelta
{
    friend class CLinearTimeAbsolute;
    friend int operator<(const CLinearTimeDelta& lta, const CLinearTimeDelta& ltb);
    friend int operator>(const CLinearTimeDelta& lta, const CLinearTimeDelta& ltb);
    friend int operator==(const CLinearTimeDelta& lta, const CLinearTimeDelta& ltb);
    friend CLinearTimeDelta operator-(const CLinearTimeAbsolute& lta, const CLinearTimeAbsolute& ltb);
    friend int operator/(const CLinearTimeDelta& ltdA, const CLinearTimeDelta& ltdB);
    friend CLinearTimeDelta operator*(const CLinearTimeDelta& ltdA, int nScaler);
    friend CLinearTimeAbsolute operator+(const CLinearTimeAbsolute& ltdA, const CLinearTimeDelta& ltdB);

private:
    INT64 m_tDelta;

public:
    CLinearTimeDelta(void);
    CLinearTimeDelta(CLinearTimeAbsolute, CLinearTimeAbsolute);

    void  ReturnTimeValueStruct(struct timeval *tv);
    long ReturnMilliseconds(void);
    long ReturnDays(void);
    long ReturnSeconds(void);

    void SetMilliseconds(unsigned long arg_dwMilliseconds);
    void SetSeconds(INT64 arg_tSeconds);
};

#endif
