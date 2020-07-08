#define SECS_PER_MIN  ((uint32_t)(60UL))
#define SECS_PER_HOUR ((uint32_t)(3600UL))
#define SECS_PER_DAY  ((uint32_t)(SECS_PER_HOUR * 24UL))
#define LEAP_YEAR(Y)  (((1970+Y)>0) && !((1970+Y)%4) && (((1970+Y)%100) || !((1970+Y)%400)))

static const char kMonthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static const uint8_t kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }; // API starts months from 1, this array starts from 0

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
