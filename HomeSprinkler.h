#pragma once

/*********************************************************************************************\
* RtcTime
\*********************************************************************************************/

// "2017-03-07T11:08:02" - ISO8601:2004
#define D_YEAR_MONTH_SEPARATOR "-"
#define D_MONTH_DAY_SEPARATOR "-"
#define D_DATE_TIME_SEPARATOR "T"
#define D_HOUR_MINUTE_SEPARATOR ":"
#define D_MINUTE_SECOND_SEPARATOR ":"

enum WeekInMonthOptions { Last, First, Second, Third, Fourth };
enum DayOfTheWeekOptions { Sun = 1, Mon, Tue, Wed, Thu, Fri, Sat };
enum MonthNamesOptions { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };
enum HemisphereOptions { North, South };
enum GetDateAndTimeOptions { DT_LOCAL, DT_UTC, DT_RESTART, DT_UPTIME };

struct TIME_T {
	uint8_t       second;
	uint8_t       minute;
	uint8_t       hour;
	uint8_t       day_of_week;                      // sunday is day 1
	uint8_t       day_of_month;
	uint8_t       month;
	char          name_of_month[4];
	uint16_t      day_of_year;
	uint16_t      year;
	unsigned long days;
	unsigned long valid;
} RtcTime;

struct TimeChangeRule
{
	uint8_t       hemis;                     // 0-Northern, 1=Southern Hemisphere (=Opposite DST/STD)
	uint8_t       week;                      // 1=First, 2=Second, 3=Third, 4=Fourth, or 0=Last week of the month
	uint8_t       dow;                       // day of week, 1=Sun, 2=Mon, ... 7=Sat
	uint8_t       month;                     // 1=Jan, 2=Feb, ... 12=Dec
	uint8_t       hour;                      // 0-23
	int           offset;                    // offset from UTC in minutes
};

enum ModStateOptions { MODE_OFF, MODE_IDLE, MODE_ACTIVE, MODE_SPRINKLING };
