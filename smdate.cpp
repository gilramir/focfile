/*
    smdate.cpp
    ----------
    Provides date storage and manipulation functions.

    Copyright (C) 1997  Gilbert Ramirez <gram@alumni.rice.edu>
    $Id: smdate.cpp,v 1.5 1997/04/24 02:35:06 gram Exp $

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "smdate.h"

#define DEBUG
#define DEBUG_PROGRAM_NAME	"SMDate"

#include "debug.h"

static char*	SMDATE_wday[7] =
	{DAY_0, DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6};

SMDATE::SMDATE(long julian, long offset) {
	set_julian(julian, offset);
};

SMDATE::SMDATE(int year, int month, int day, long offset) {
	set_ymd(year, month, day, offset);
};

void SMDATE::set_julian(long julian, long offset) {

	/* Store the data */
	Date_julian	= julian + offset;
	Offset_days	= offset;

	reset_cache();
}

void SMDATE::reset_cache(void) {
	/* Initialize variables */
	Date_month	= -1;
	Date_mday	= -1;
	Date_year	= -1;
	Date_yday	= -1;
	Date_wday	= -1;
}


int SMDATE::mday(void) {

	if (Date_mday < 0) {
		calculate_mdy();
	}
	return Date_mday;
}

int SMDATE::month(void) {

	if (Date_month < 0) {
		calculate_mdy();
	}
	return Date_month;
}

int SMDATE::year(void) {
	if (Date_year < 0) {
		calculate_mdy();
	}
	return Date_year;
}

char* SMDATE::wdayname(void) {
	return SMDATE_wday[wday()];
}

/*
char*	LongMonthName[12] = {LONG_MONTH_1, LONG_MONTH_2, LONG_MONTH_3,
		LONG_MONTH_4, LONG_MONTH_5, LONG_MONTH_6, LONG_MONTH_7,
		LONG_MONTH_8, LONG_MONTH_9, LONG_MONTH_10, LONG_MONTH_11, 
		LONG_MONTH_12};
char*	ShortMonthName[12] = {SHORT_MONTH_1, SHORT_MONTH_2, SHORT_MONTH_3,
		SHORT_MONTH_4, SHORT_MONTH_5, SHORT_MONTH_6, SHORT_MONTH_7,
		SHORT_MONTH_8, SHORT_MONTH_9, SHORT_MONTH_10, SHORT_MONTH_11, 
		SHORT_MONTH_12};
*/

/* These routines were taken from the IDL Astronomy Users's Library
	http://idlastro.gsfc.nasa.gov/homepage.html

   The time routines were written for IDL by William Thompson, 13 Sep 1993,
	from GSFC, NASA.  Thompson cites the algorithm by Fliegel and
	Van Flandern (1968) reprinted in the Explanatory Supplement to the
	Astronomical Almanac, 1992.

   The functions were translated from IDL to C++ by Gilbert Ramirez.
   	<gram@alumni.rice.edu> April 15, 1997
*/

// Calculates month, day, and year, from the Modified Julian Day, which
// is the number of days since 1858-Nov-17.
void SMDATE::calculate_mdy(void) {

	long	jd, l, n, year, month;

	jd = Date_julian + 2400001;

	l = jd + 68569;
	n = 4 * l / 146097;
	l = l - (146097 * n + 3) / 4;
	year = 4000 * (l + 1) / 1461001;
	l = l - 1461 * year / 4 + 31;
	month = 80 * l / 2447;
	Date_mday = l - 2447 * month / 80;
	l = month / 11;
	Date_month = month + 2 - 12 * l;
	Date_year = 100 * (n - 49) + year + l;
}

void SMDATE::set_ymd(short y, short m, short d, long offset) {

	Offset_days	= offset;
	reset_cache();

	long l = (m - 14) / 12;

	Date_julian = (long) d - 2432076L + 1461 *
			((long) y + 4800 + l) / 4 +
			367 * (m - 2 - l * 12) / 12 -
			3 * ((y + 4900 + l) / 100) / 4;
}

int SMDATE::wday(void) {
	if (Date_wday < 0) {
		Date_wday = (Date_julian + 3) % 7;
	}
	return Date_wday;
}

/* ------------------------------------------------------------------- */
/* -------------------- END OF TRANSLATED IDL CODE ------------------- */
/* ------------------------------------------------------------------- */


/* The following routines are copied and only slightly modified from YACL,
   "Yet Another Class Library". YACL is copyright by M. A. Sridhar, and is
   free software:

 *
 *          Copyright (C) 1995, M. A. Sridhar
 *  
 *
 *     This software is Copyright M. A. Sridhar, 1995. You are free
 *     to copy, modify or distribute this software  as you see fit,
 *     and to use  it  for  any  purpose, provided   this copyright
 *     notice and the following   disclaimer are included  with all
 *     copies.
 *
 *                        DISCLAIMER
 *
 *     The author makes no warranties, either expressed or implied,
 *     with respect  to  this  software, its  quality, performance,
 *     merchantability, or fitness for any particular purpose. This
 *     software is distributed  AS IS.  The  user of this  software
 *     assumes all risks  as to its quality  and performance. In no
 *     event shall the author be liable for any direct, indirect or
 *     consequential damages, even if the  author has been  advised
 *     as to the possibility of such damages.
 *
 */

#if defined (__GNUC__)
extern "C" time_t time (time_t*);
#endif

SMDATE SMDATE::today(long offset)
{
    time_t	timer;
    struct tm	*tblock;
    short	y, m, d;

    timer = time ((time_t*) NULL);
    tblock = localtime (&timer);
    y = tblock->tm_year + 1900; 
    m = tblock->tm_mon + 1; // Correct for 0-11
    d = tblock->tm_mday;
    return SMDATE(y, m, d, offset);
}

SMDATE  SMDATE::operator+  (long num_days) const {
//    return SMDATE(Date_julian - Offset_days + num_days, Offset_days);
	return SMDATE(julian() + num_days, Offset_days);
}

SMDATE&	SMDATE::operator+= (long num_days) {

	Date_julian += num_days;
	reset_cache();
	return *this;
}

long SMDATE::operator- (const SMDATE& d) const {

	return Date_julian - d.internal();
}


// -----------------------------------------------------------------
// End of borrowed YACL code.
// -----------------------------------------------------------------

/* magic settings for vi editors
vi:set ts=8:
vi:set sw=8:
*/
