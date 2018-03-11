/*********************************************************************************************\
* Wifi
\*********************************************************************************************/

enum WifiConfigOptions { WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG, WIFI_RETRY, WIFI_WAIT, MAX_WIFI_OPTION };

struct SYSCFG {
	unsigned long cfg_holder;                // 000
	unsigned long save_flag;                 // 004
	unsigned long version;                   // 008
	unsigned long bootcount;                 // 00C
	int16_t       save_data;                 // 014
	int8_t        timezone;                  // 016

	byte          seriallog_level;           // 09E
	uint8_t       sta_config;                // 09F
	byte          sta_active;                // 0A0
	char          sta_ssid[2][33];           // 0A1
	char          sta_pwd[2][65];            // 0E3
	char          hostname[33];              // 165
	char          syslog_host[33];           // 186
	uint16_t      syslog_port;               // 1A8
	byte          syslog_level;              // 1AA
	uint8_t       webserver;                 // 1AB
	byte          weblog_level;              // 1AC
	char          mqtt_fingerprint[60];      // 1AD To be freed by binary fingerprint
	char          mqtt_host[33];             // 1E9
	uint16_t      mqtt_port;                 // 20A
	char          mqtt_client[33];           // 20C
	char          mqtt_user[33];             // 22D
	char          mqtt_pwd[33];              // 24E
	char          mqtt_topic[33];            // 26F
	char          button_topic[33];          // 290
	char          mqtt_grptopic[33];         // 2B1

	uint16_t      tele_period;               // 2F8
	uint8_t       ledstate;                  // 2FB
	uint16_t      mqtt_retry;                // 396
	uint8_t       poweronstate;              // 398
	uint8_t       last_module;               // 399

	uint16_t      blinktime;                 // 39A
	uint16_t      blinkcount;                // 39C
	char          web_password[33];          // 4A9
	char          ntp_server[3][33];         // 4CE

	uint32_t      ip_address[4];             // 544

	char          mqtt_fulltopic[100];       // 558

} WifiSettings;
