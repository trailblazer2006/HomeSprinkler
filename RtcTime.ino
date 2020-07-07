/*********************************************************************************************\
* Real Time Clock
*
* Sources: Time by Michael Margolis and Paul Stoffregen (https://github.com/PaulStoffregen/Time)
*          Timezone by Jack Christensen (https://github.com/JChristensen/Timezone)
\*********************************************************************************************/

extern "C" {
#include "sntp.h"
}

#define SECS_PER_MIN  ((uint32_t)(60UL))
#define SECS_PER_HOUR ((uint32_t)(3600UL))
#define SECS_PER_DAY  ((uint32_t)(SECS_PER_HOUR * 24UL))
#define LEAP_YEAR(Y)  (((1970+Y)>0) && !((1970+Y)%4) && (((1970+Y)%100) || !((1970+Y)%400)))

Ticker TickerRtc;

static const char kMonthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static const uint8_t kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }; // API starts months from 1, this array starts from 0

TimeChangeRule DaylightSavingTime = { TIME_DST }; // Daylight Saving Time
TimeChangeRule StandardTime = { TIME_STD }; // Standard Time

uint32_t utc_time = 0;
uint32_t local_time = 0;
uint32_t daylight_saving_time = 0;
uint32_t standard_time = 0;
uint32_t ntp_time = 0;
uint32_t midnight = 1451602800;
uint32_t restart_time = 0;
uint8_t  midnight_now = 0;
uint8_t  ntp_sync_minute = 0;

String GetBuildDateAndTime()
{
	// "2017-03-07T11:08:02" - ISO8601:2004
	char bdt[21];
	char *p;
	char mdate[] = __DATE__;  // "Mar  7 2017"
	char *smonth = mdate;
	int day = 0;
	int year = 0;

	// sscanf(mdate, "%s %d %d", bdt, &day, &year);  // Not implemented in 2.3.0 and probably too much code
	byte i = 0;
	for (char *str = strtok_r(mdate, " ", &p); str && i < 3; str = strtok_r(NULL, " ", &p)) {
		switch (i++) {
		case 0:  // Month
			smonth = str;
			break;
		case 1:  // Day
			day = atoi(str);
			break;
		case 2:  // Year
			year = atoi(str);
		}
	}
	int month = (strstr(kMonthNames, smonth) - kMonthNames) / 3 + 1;
	snprintf_P(bdt, sizeof(bdt), PSTR("%d" D_YEAR_MONTH_SEPARATOR "%02d" D_MONTH_DAY_SEPARATOR "%02d" D_DATE_TIME_SEPARATOR "%s"), year, month, day, __TIME__);
	return String(bdt);
}

String GetDateAndTime(byte time_type)
{
	// enum GetDateAndTimeOptions { DT_LOCAL, DT_UTC, DT_RESTART, DT_UPTIME };
	// "2017-03-07T11:08:02" - ISO8601:2004
	char dt[21];
	TIME_T tmpTime;

	if (DT_UPTIME == time_type) {
		if (restart_time) {
			BreakTime(utc_time - restart_time, tmpTime);
		}
		else {
			BreakTime(uptime, tmpTime);
		}
		// "P128DT14H35M44S" - ISO8601:2004 - https://en.wikipedia.org/wiki/ISO_8601 Durations
		// snprintf_P(dt, sizeof(dt), PSTR("P%dDT%02dH%02dM%02dS"), ut.days, ut.hour, ut.minute, ut.second);
		// "128 14:35:44" - OpenVMS
		// "128T14:35:44" - Tasmota
		snprintf_P(dt, sizeof(dt), PSTR("%dT%02d:%02d:%02d"),
			tmpTime.days, tmpTime.hour, tmpTime.minute, tmpTime.second);
	}
	else {
		switch (time_type) {
		case DT_UTC:
			BreakTime(utc_time, tmpTime);
			tmpTime.year += 1970;
			break;
		case DT_RESTART:
			if (restart_time == 0) {
				return "";
			}
			BreakTime(restart_time, tmpTime);
			tmpTime.year += 1970;
			break;
		default:
			tmpTime = RtcTime;
		}
		snprintf_P(dt, sizeof(dt), PSTR("%04d-%02d-%02dT%02d:%02d:%02d"),
			tmpTime.year, tmpTime.month, tmpTime.day_of_month, tmpTime.hour, tmpTime.minute, tmpTime.second);
	}
	return String(dt);
}

String GetUptime()
{
	char dt[16];

	TIME_T ut;

	if (restart_time) {
		BreakTime(utc_time - restart_time, ut);
	}
	else {
		BreakTime(uptime, ut);
	}

	// "P128DT14H35M44S" - ISO8601:2004 - https://en.wikipedia.org/wiki/ISO_8601 Durations
	//  snprintf_P(dt, sizeof(dt), PSTR("P%dDT%02dH%02dM%02dS"), ut.days, ut.hour, ut.minute, ut.second);

	// "128 14:35:44" - OpenVMS
	// "128T14:35:44" - Tasmota
	snprintf_P(dt, sizeof(dt), PSTR("%dT%02d:%02d:%02d"), ut.days, ut.hour, ut.minute, ut.second);
	return String(dt);
}

void BreakTime(uint32_t time_input, TIME_T &tm)
{
	// break the given time_input into time components
	// this is a more compact version of the C library localtime function
	// note that year is offset from 1970 !!!

	uint8_t year;
	uint8_t month;
	uint8_t month_length;
	uint32_t time;
	unsigned long days;

	time = time_input;
	tm.second = time % 60;
	time /= 60;                // now it is minutes
	tm.minute = time % 60;
	time /= 60;                // now it is hours
	tm.hour = time % 24;
	time /= 24;                // now it is days
	tm.days = time;
	tm.day_of_week = ((time + 4) % 7) + 1;  // Sunday is day 1

	year = 0;
	days = 0;
	while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
		year++;
	}
	tm.year = year;            // year is offset from 1970

	days -= LEAP_YEAR(year) ? 366 : 365;
	time -= days;              // now it is days in this year, starting at 0
	tm.day_of_year = time;

	days = 0;
	month = 0;
	month_length = 0;
	for (month = 0; month < 12; month++) {
		if (1 == month) { // february
			if (LEAP_YEAR(year)) {
				month_length = 29;
			}
			else {
				month_length = 28;
			}
		}
		else {
			month_length = kDaysInMonth[month];
		}

		if (time >= month_length) {
			time -= month_length;
		}
		else {
			break;
		}
	}
	strlcpy(tm.name_of_month, kMonthNames + (month * 3), 4);
	tm.month = month + 1;      // jan is month 1
	tm.day_of_month = time + 1;         // day of month
	tm.valid = (time_input > 1451602800);  // 2016-01-01
}

uint32_t MakeTime(TIME_T &tm)
{
	// assemble time elements into time_t
	// note year argument is offset from 1970

	int i;
	uint32_t seconds;

	// seconds from 1970 till 1 jan 00:00:00 of the given year
	seconds = tm.year * (SECS_PER_DAY * 365);
	for (i = 0; i < tm.year; i++) {
		if (LEAP_YEAR(i)) {
			seconds += SECS_PER_DAY;   // add extra days for leap years
		}
	}

	// add days for this year, months start from 1
	for (i = 1; i < tm.month; i++) {
		if ((2 == i) && LEAP_YEAR(tm.year)) {
			seconds += SECS_PER_DAY * 29;
		}
		else {
			seconds += SECS_PER_DAY * kDaysInMonth[i - 1];  // monthDay array starts from 0
		}
	}
	seconds += (tm.day_of_month - 1) * SECS_PER_DAY;
	seconds += tm.hour * SECS_PER_HOUR;
	seconds += tm.minute * SECS_PER_MIN;
	seconds += tm.second;
	return seconds;
}

uint32_t RuleToTime(TimeChangeRule r, int yr)
{
	TIME_T tm;
	uint32_t t;
	uint8_t m;
	uint8_t w;                // temp copies of r.month and r.week

	m = r.month;
	w = r.week;
	if (0 == w) {             // Last week = 0
		if (++m > 12) {         // for "Last", go to the next month
			m = 1;
			yr++;
		}
		w = 1;                  // and treat as first week of next month, subtract 7 days later
	}

	tm.hour = r.hour;
	tm.minute = 0;
	tm.second = 0;
	tm.day_of_month = 1;
	tm.month = m;
	tm.year = yr - 1970;
	t = MakeTime(tm);         // First day of the month, or first day of next month for "Last" rules
	BreakTime(t, tm);
	t += (7 * (w - 1) + (r.dow - tm.day_of_week + 7) % 7) * SECS_PER_DAY;
	if (0 == r.week) {
		t -= 7 * SECS_PER_DAY;  // back up a week if this is a "Last" rule
	}
	return t;
}

String GetTime(int type)
{
	char stime[25];   // Skip newline

	uint32_t time = utc_time;
	if (1 == type) time = local_time;
	if (2 == type) time = daylight_saving_time;
	if (3 == type) time = standard_time;
	snprintf_P(stime, sizeof(stime), sntp_get_real_time(time));
	return String(stime);
}

uint32_t LocalTime()
{
	return local_time;
}

uint32_t Midnight()
{
	return midnight;
}

boolean MidnightNow()
{
	boolean mnflg = midnight_now;
	if (mnflg) midnight_now = 0;
	return mnflg;
}

void RtcSecond()
{
	uint32_t stdoffset;
	uint32_t dstoffset;
	TIME_T tmpTime;

	if ((ntp_sync_minute > 59) && (RtcTime.minute > 2)) ntp_sync_minute = 1;                 // If sync prepare for a new cycle
	uint8_t offset = (uptime < 30) ? RtcTime.second : (((ESP.getChipId() & 0xF) * 3) + 3);  // First try ASAP to sync. If fails try once every 60 seconds based on chip id
	if ((WL_CONNECTED == WiFi.status()) && (offset == RtcTime.second) && ((RtcTime.year < 2016) || (ntp_sync_minute == RtcTime.minute))) {
		ntp_time = sntp_get_current_timestamp();
		if (ntp_time) {
			utc_time = ntp_time;
			ntp_sync_minute = 60;  // Sync so block further requests
			if (restart_time == 0) {
				restart_time = utc_time - uptime;  // save first ntp time as restart time
			}
			BreakTime(utc_time, tmpTime);
			RtcTime.year = tmpTime.year + 1970;
			daylight_saving_time = RuleToTime(DaylightSavingTime, RtcTime.year);
			standard_time = RuleToTime(StandardTime, RtcTime.year);
		}
		else {
			ntp_sync_minute++;  // Try again in next minute
		}
	}
	utc_time++;
	local_time = utc_time;
	if (local_time > 1451602800) {  // 2016-01-01
		if (99 == APP_TIMEZONE) {
			if (DaylightSavingTime.hemis) {
				dstoffset = StandardTime.offset * SECS_PER_MIN;  // Southern hemisphere
				stdoffset = DaylightSavingTime.offset * SECS_PER_MIN;
			}
			else {
				dstoffset = DaylightSavingTime.offset * SECS_PER_MIN;  // Northern hemisphere
				stdoffset = StandardTime.offset * SECS_PER_MIN;
			}
			if ((utc_time >= (daylight_saving_time - stdoffset)) && (utc_time < (standard_time - dstoffset))) {
				local_time += dstoffset;  // Daylight Saving Time
			}
			else {
				local_time += stdoffset;  // Standard Time
			}
		}
		else {
			local_time += APP_TIMEZONE * SECS_PER_HOUR;
		}
	}
	BreakTime(local_time, RtcTime);
	if (!RtcTime.hour && !RtcTime.minute && !RtcTime.second && RtcTime.valid) {
		midnight = local_time;
		midnight_now = 1;
	}
	RtcTime.year += 1970;
}

void RtcInit()
{
	sntp_setservername(0, NTP_SERVER1);
	sntp_setservername(1, NTP_SERVER2);
	sntp_setservername(2, NTP_SERVER3);
	sntp_stop();
	sntp_set_timezone(0);      // UTC time
	sntp_init();
	utc_time = 0;
	BreakTime(utc_time, RtcTime);
	TickerRtc.attach(1, RtcSecond);
}
