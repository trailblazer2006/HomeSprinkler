#pragma once

// -- Project -------------------------------------
#define PROJECT                "HomeSprinkler"   // PROJECT is used as the default topic delimiter and OTA file name
//   As an IDE restriction it needs to be the same as the main .ino file

// -- Wifi ----------------------------------------
#define WIFI_HOSTNAME          "%s-%04d"         // Expands to <PROJECT>-<last 4 decimal chars of MAC address>

// -- Time - Up to three NTP servers in your region
#define NTP_SERVER1            "pool.ntp.org"       // [NtpServer1] Select first NTP server by name or IP address (129.250.35.250)
#define NTP_SERVER2            "nl.pool.ntp.org"    // [NtpServer2] Select second NTP server by name or IP address (5.39.184.5)
#define NTP_SERVER3            "0.nl.pool.ntp.org"  // [NtpServer3] Select third NTP server by name or IP address (93.94.224.67)

// -- Time - Start Daylight Saving Time and timezone offset from UTC in minutes
#define TIME_DST               North, Last, Sun, Mar, 2, +120  // Northern Hemisphere, Last sunday in march at 02:00 +120 minutes

// -- Time - Start Standard Time and timezone offset from UTC in minutes
#define TIME_STD               North, Last, Sun, Oct, 3, +60   // Northern Hemisphere, Last sunday in october 02:00 +60 minutes

// -- Application ---------------------------------
#define APP_TIMEZONE           3                 // [Timezone] +1 hour (Amsterdam) (-12 .. 12 = hours from UTC, 99 = use TIME_DST/TIME_STD)
