#pragma once

#define APP_TIMEZONE           3                    // [Timezone] +1 hour (Amsterdam) (-12 .. 12 = hours from UTC, 99 = use TIME_DST/TIME_STD)
// -- Time - Up to three NTP servers in your region
#define NTP_SERVER1            "pool.ntp.org"       // [NtpServer1] Select first NTP server by name or IP address (129.250.35.250)
#define NTP_SERVER2            "nl.pool.ntp.org"    // [NtpServer2] Select second NTP server by name or IP address (5.39.184.5)
#define NTP_SERVER3            "0.nl.pool.ntp.org"  // [NtpServer3] Select third NTP server by name or IP address (93.94.224.67)

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
};

struct TimeChangeRule
{
	uint8_t       hemis;                     // 0-Northern, 1=Southern Hemisphere (=Opposite DST/STD)
	uint8_t       week;                      // 1=First, 2=Second, 3=Third, 4=Fourth, or 0=Last week of the month
	uint8_t       dow;                       // day of week, 1=Sun, 2=Mon, ... 7=Sat
	uint8_t       month;                     // 1=Jan, 2=Feb, ... 12=Dec
	uint8_t       hour;                      // 0-23
	int           offset;                    // offset from UTC in minutes
};

// -- Time - Start Daylight Saving Time and timezone offset from UTC in minutes
#define TIME_DST               North, Last, Sun, Mar, 2, +120  // Northern Hemisphere, Last sunday in march at 02:00 +120 minutes

// -- Time - Start Standard Time and timezone offset from UTC in minutes
#define TIME_STD               North, Last, Sun, Oct, 3, +60   // Northern Hemisphere, Last sunday in october 02:00 +60 minutes

TimeChangeRule DaylightSavingTime = { TIME_DST }; // Daylight Saving Time
TimeChangeRule StandardTime = { TIME_STD }; // Standard Time
