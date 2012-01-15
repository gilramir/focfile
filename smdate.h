/*
    smdate.h
    --------
    Provides date storage and manipulation functions.

    Copyright (C) 1997  Gilbert Ramirez <gram@alumni.rice.edu>
    $Id: smdate.h,v 1.6 1997/04/24 02:35:06 gram Exp $

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

#ifndef SMDATE_H
#define SMDATE_H

/* All dates in SMDATE are internally stored as offsets from 1858-Nov-17.
   When creating an SMDATE object, you can use these pre-defined offsets
   so that your program can use different Julian dates (i.e., w/ different
   offsets. However, internally, the offset is still 1858-Nov-17. */

// SMDATE_DEFAULT is the default offset
#define SMDATE_DEFAULT		15384
#define SMDATE_NOV17_1858	0	// Days since 1858-Nov-17
#define SMDATE_FOCUS		15384	// Days since 1900-Dec-31


class SMDATE {

public:
	SMDATE(long jul = 0, long offset = SMDATE_DEFAULT);
	SMDATE(int y, int m, int d, long offset = SMDATE_DEFAULT);
//	SMDATE(char*, int, int, int);
//	SMDATE(char*, char*);
//	~SMDATE();

	// Return the user's concept of the Julian date.
	long	julian(void) const {return Date_julian - Offset_days;};

	int 	mday(void);		// 1-28,29,30,31
	int 	month(void);		// 1-12
	int	year(void);		// full year
	int	wday(void);		// 1-7
	char*	wdayname(void);		// 
	int	yday(void);		// 1-365,366
	int	is_leap_year(void);	// 0,1

	int 	zmonth(void) {return month() - 1; };	// 0-11
	int	zwday(void) {return wday() - 1;};	// 0-6

	void	get_mdy(int*, int*, int*);
	static SMDATE	today(long offset = SMDATE_DEFAULT);

	void	set_julian(long, long offset = SMDATE_DEFAULT);
	void	set_ymd(short y, short m, short d,
				long offset = SMDATE_DEFAULT);
	long	internal(void) const { return Date_julian; };

	int	operator<  (const SMDATE& d) const
		{return Date_julian < d.internal() ? 1 : 0;};

	int	operator<=  (const SMDATE& d) const
		{return Date_julian <= d.internal() ? 1 : 0;};

	int	operator>  (const SMDATE& d) const
		{return Date_julian > d.internal() ? 1 : 0;};

	int	operator>=  (const SMDATE& d) const
		{return Date_julian >= d.internal() ? 1 : 0;};

	int	operator==  (const SMDATE& d) const
		{return Date_julian == d.internal() ? 1 : 0;};

	int	operator!=  (const SMDATE& d) const
		{return Date_julian != d.internal() ? 1 : 0;};

	SMDATE			operator+  (long num_days) const;
	virtual SMDATE&		operator+= (long num_days);
	SMDATE			operator-  (long num_days) const
				{ return operator+(-num_days);};
	virtual SMDATE&		operator-= (long num_days)
				{ return operator+=(-num_days);};
	long			operator-  (const SMDATE& d) const;

private:
	void	calculate_mdy(void);
	void	reset_cache(void);

	long	Offset_days;
	long	Date_julian;	// # days since Nov 17, 1858
	int	Date_month;
	int	Date_mday;
	int	Date_year;
	int	Date_yday;
	int	Date_wday;
};

/*

t_date		Julian(int, int, int);
int		WeekDayNo(t_date);
int		CheckDate(int, int, int);
int		CorrectDate(int*, int*, int*);

extern char* LongMonthName[];
extern char* ShortMonthName[];
*/


#define DAY_0			"Sunday"
#define DAY_1			"Monday"
#define DAY_2			"Tuesday"
#define DAY_3			"Wednesday"
#define DAY_4			"Thursday"
#define DAY_5			"Friday"
#define DAY_6			"Saturday"

#define LONG_MONTH_1		"January"
#define LONG_MONTH_2		"February"
#define LONG_MONTH_3		"March"
#define	LONG_MONTH_4		"April"
#define LONG_MONTH_5		"May"
#define LONG_MONTH_6		"June"
#define LONG_MONTH_7		"July"
#define LONG_MONTH_8		"August"
#define LONG_MONTH_9		"September"
#define LONG_MONTH_10		"October"
#define LONG_MONTH_11		"November"
#define LONG_MONTH_12		"December"

#define SHORT_MONTH_WIDTH	3
#define SHORT_MONTH_1		"Jan"
#define SHORT_MONTH_2		"Feb"
#define SHORT_MONTH_3 		"Mar"
#define SHORT_MONTH_4 		"Apr"
#define SHORT_MONTH_5 		"May"
#define SHORT_MONTH_6 		"Jun"
#define SHORT_MONTH_7 		"Jul"
#define SHORT_MONTH_8 		"Aug"
#define SHORT_MONTH_9 		"Sep"
#define SHORT_MONTH_10 		"Oct"
#define SHORT_MONTH_11 		"Nov"
#define SHORT_MONTH_12 		"Dec"


#endif /* SMDATE_H */

/* magic settings for vi editors
vi:set ts=8:
vi:set sw=8:
*/
