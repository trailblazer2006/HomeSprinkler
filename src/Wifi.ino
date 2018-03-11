/*********************************************************************************************\
* Wifi
\*********************************************************************************************/

#define WIFI_CONFIG_SEC   180  // seconds before restart
#define WIFI_CHECK_SEC    20   // seconds
#define WIFI_RETRY_SEC    30   // seconds

uint8_t wifi_counter;          // Number of seconds until check wifi
uint8_t wifi_retry;            // Number of seconds until wifi connection attempt (at half will be an attempt to connect to alternative wifi)
uint8_t wifi_status;
uint8_t wps_result;
uint8_t wifi_config_type = 0;
uint8_t wifi_config_counter = 0;

int WifiGetRssiAsQuality(int rssi)
{
	int quality = 0;

	if (rssi <= -100) {
		quality = 0;
	}
	else if (rssi >= -50) {
		quality = 100;
	}
	else {
		quality = 2 * (rssi + 100);
	}
	return quality;
}

boolean WifiConfigCounter()
{
	if (wifi_config_counter) {
		wifi_config_counter = WIFI_CONFIG_SEC;
	}
	return (wifi_config_counter);
}

extern "C" {
#include "user_interface.h"
}

void WifiWpsStatusCallback(wps_cb_status status);

void WifiWpsStatusCallback(wps_cb_status status)
{
	/* from user_interface.h:
	enum wps_cb_status {
	WPS_CB_ST_SUCCESS = 0,
	WPS_CB_ST_FAILED,
	WPS_CB_ST_TIMEOUT,
	WPS_CB_ST_WEP,      // WPS failed because that WEP is not supported
	WPS_CB_ST_SCAN_ERR, // can not find the target WPS AP
	};
	*/
	wps_result = status;
	if (WPS_CB_ST_SUCCESS == wps_result) {
		wifi_wps_disable();
	}
	else {
		//snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_WIFI D_WPS_FAILED_WITH_STATUS " %d"), wps_result);
		//AddLog(LOG_LEVEL_DEBUG);
		wifi_config_counter = 2;
	}
}

boolean WifiWpsConfigDone(void)
{
	return (!wps_result);
}

boolean WifiWpsConfigBegin(void)
{
	wps_result = 99;
	if (!wifi_wps_disable()) {
		return false;
	}
	if (!wifi_wps_enable(WPS_TYPE_PBC)) {
		return false;  // so far only WPS_TYPE_PBC is supported (SDK 2.0.0)
	}
	if (!wifi_set_wps_cb((wps_st_cb_t)&WifiWpsStatusCallback)) {
		return false;
	}
	if (!wifi_wps_start()) {
		return false;
	}
	return true;
}

void WifiConfig(uint8_t type)
{
	if (!wifi_config_type) {
		if (type >= WIFI_RETRY) {  // WIFI_RETRY and WIFI_WAIT
			return;
		}
		WiFi.disconnect();        // Solve possible Wifi hangs
		wifi_config_type = type;
		wifi_config_counter = WIFI_CONFIG_SEC;   // Allow up to WIFI_CONFIG_SECS seconds for phone to provide ssid/pswd
		wifi_counter = wifi_config_counter + 5;
		blinker.attach(0.2, blink);
		if (WIFI_RESTART == wifi_config_type) {
			restart_flag = 2;
		}
		else if (WIFI_SMARTCONFIG == wifi_config_type) {
			//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_1_SMARTCONFIG " " D_ACTIVE_FOR_3_MINUTES));
			WiFi.beginSmartConfig();
		}
		else if (WIFI_WPSCONFIG == wifi_config_type) {
			if (WifiWpsConfigBegin()) {
				//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_3_WPSCONFIG " " D_ACTIVE_FOR_3_MINUTES));
			}
			else {
				//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_3_WPSCONFIG " " D_FAILED_TO_START));
				wifi_config_counter = 3;
			}
		}
#ifdef USE_WEBSERVER
		else if (WIFI_MANAGER == wifi_config_type) {
			AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_WCFG_2_WIFIMANAGER " " D_ACTIVE_FOR_3_MINUTES));
			WifiManagerBegin();
		}
#endif  // USE_WEBSERVER
	}
}

void WifiBegin(uint8_t flag)
{
	const char kWifiPhyMode[] = " BGN";

#ifdef ARDUINO_ESP8266_RELEASE_2_3_0  // (!strncmp_P(ESP.getSdkVersion(),PSTR("1.5.3"),5))
	AddLog_P(LOG_LEVEL_DEBUG, S_LOG_WIFI, PSTR(D_PATCH_ISSUE_2186));
	WiFi.mode(WIFI_OFF);      // See https://github.com/esp8266/Arduino/issues/2186
#endif

	WiFi.disconnect(true);    // Delete SDK wifi config
	delay(200);
	WiFi.mode(WIFI_STA);      // Disable AP mode
	//if (Settings.sleep) {
	//	WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // Allow light sleep during idle times
	//}
	//  if (WiFi.getPhyMode() != WIFI_PHY_MODE_11N) {
	//    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
	//  }
	if (!WiFi.getAutoConnect()) {
		WiFi.setAutoConnect(true);
	}
	//  WiFi.setAutoReconnect(true);
	switch (flag) {
	case 0:  // AP1
	case 1:  // AP2
		WifiSettings.sta_active = flag;
		break;
	case 2:  // Toggle
		WifiSettings.sta_active ^= 1;
	}        // 3: Current AP
	if (0 == strlen(WifiSettings.sta_ssid[1])) {
		WifiSettings.sta_active = 0;
	}
	if (WifiSettings.ip_address[0]) {
		WiFi.config(WifiSettings.ip_address[0], WifiSettings.ip_address[1], WifiSettings.ip_address[2], WifiSettings.ip_address[3]);  // Set static IP
	}
	WiFi.hostname(my_hostname);
	WiFi.begin(WifiSettings.sta_ssid[WifiSettings.sta_active], WifiSettings.sta_pwd[WifiSettings.sta_active]);
	//snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_WIFI D_CONNECTING_TO_AP "%d %s " D_IN_MODE " 11%c " D_AS " %s..."),
	//	Settings.sta_active + 1, Settings.sta_ssid[Settings.sta_active], kWifiPhyMode[WiFi.getPhyMode() & 0x3], my_hostname);
	//AddLog(LOG_LEVEL_INFO);
}

void WifiCheckIp()
{
	if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0)) {
		wifi_counter = WIFI_CHECK_SEC;
		wifi_retry = WIFI_RETRY_SEC;
		//AddLog_P((wifi_status != WL_CONNECTED) ? LOG_LEVEL_INFO : LOG_LEVEL_DEBUG_MORE, S_LOG_WIFI, PSTR(D_CONNECTED));
		if (wifi_status != WL_CONNECTED) {
			//      AddLog_P(LOG_LEVEL_INFO, PSTR("Wifi: Set IP addresses"));
			WifiSettings.ip_address[1] = (uint32_t)WiFi.gatewayIP();
			WifiSettings.ip_address[2] = (uint32_t)WiFi.subnetMask();
			WifiSettings.ip_address[3] = (uint32_t)WiFi.dnsIP();
		}
		wifi_status = WL_CONNECTED;
	}
	else {
		wifi_status = WiFi.status();
		switch (wifi_status) {
		case WL_CONNECTED:
			//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_NO_IP_ADDRESS));
			wifi_status = 0;
			wifi_retry = WIFI_RETRY_SEC;
			break;
		case WL_NO_SSID_AVAIL:
			//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_AP_NOT_REACHED));
			if (WIFI_WAIT == WifiSettings.sta_config) {
				wifi_retry = WIFI_RETRY_SEC;
			}
			else {
				if (wifi_retry > (WIFI_RETRY_SEC / 2)) {
					wifi_retry = WIFI_RETRY_SEC / 2;
				}
				else if (wifi_retry) {
					wifi_retry = 0;
				}
			}
			break;
		case WL_CONNECT_FAILED:
			//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_WRONG_PASSWORD));
			if (wifi_retry > (WIFI_RETRY_SEC / 2)) {
				wifi_retry = WIFI_RETRY_SEC / 2;
			}
			else if (wifi_retry) {
				wifi_retry = 0;
			}
			break;
		default:  // WL_IDLE_STATUS and WL_DISCONNECTED
			if (!wifi_retry || ((WIFI_RETRY_SEC / 2) == wifi_retry)) {
				//AddLog_P(LOG_LEVEL_INFO, S_LOG_WIFI, PSTR(D_CONNECT_FAILED_AP_TIMEOUT));
			}
			else {
				//AddLog_P(LOG_LEVEL_DEBUG, S_LOG_WIFI, PSTR(D_ATTEMPTING_CONNECTION));
			}
		}
		if (wifi_retry) {
			if (WIFI_RETRY_SEC == wifi_retry) {
				WifiBegin(3);  // Select default SSID
			}
			if ((WifiSettings.sta_config != WIFI_WAIT) && ((WIFI_RETRY_SEC / 2) == wifi_retry)) {
				WifiBegin(2);  // Select alternate SSID
			}
			wifi_counter = 1;
			wifi_retry--;
		}
		else {
			WifiConfig(WifiSettings.sta_config);
			wifi_counter = 1;
			wifi_retry = WIFI_RETRY_SEC;
		}
	}
}

void WifiCheck(uint8_t param)
{
	wifi_counter--;
	switch (param) {
	case WIFI_SMARTCONFIG:
	case WIFI_MANAGER:
	case WIFI_WPSCONFIG:
		WifiConfig(param);
		break;
	default:
		if (wifi_config_counter) {
			wifi_config_counter--;
			wifi_counter = wifi_config_counter + 5;
			if (wifi_config_counter) {
				if ((WIFI_SMARTCONFIG == wifi_config_type) && WiFi.smartConfigDone()) {
					wifi_config_counter = 0;
				}
				if ((WIFI_WPSCONFIG == wifi_config_type) && WifiWpsConfigDone()) {
					wifi_config_counter = 0;
				}
				if (!wifi_config_counter) {
					if (strlen(WiFi.SSID().c_str())) {
						strlcpy(WifiSettings.sta_ssid[0], WiFi.SSID().c_str(), sizeof(WifiSettings.sta_ssid[0]));
					}
					if (strlen(WiFi.psk().c_str())) {
						strlcpy(WifiSettings.sta_pwd[0], WiFi.psk().c_str(), sizeof(WifiSettings.sta_pwd[0]));
					}
					WifiSettings.sta_active = 0;
					//snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_WIFI D_WCFG_1_SMARTCONFIG D_CMND_SSID "1 %s, " D_CMND_PASSWORD "1 %s"), Settings.sta_ssid[0], Settings.sta_pwd[0]);
					//AddLog(LOG_LEVEL_INFO);
				}
			}
			if (!wifi_config_counter) {
				if (WIFI_SMARTCONFIG == wifi_config_type) {
					WiFi.stopSmartConfig();
				}
				SettingsSdkErase();
				restart_flag = 2;
			}
		}
		else {
			if (wifi_counter <= 0) {
				//AddLog_P(LOG_LEVEL_DEBUG_MORE, S_LOG_WIFI, PSTR(D_CHECKING_CONNECTION));
				wifi_counter = WIFI_CHECK_SEC;
				WifiCheckIp();
			}
			if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0) && !wifi_config_type) {
				blinker.detach();
#ifdef USE_DISCOVERY
				if (!mdns_begun) {
					mdns_begun = MDNS.begin(my_hostname);
					snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_MDNS "%s"), (mdns_begun) ? D_INITIALIZED : D_FAILED);
					AddLog(LOG_LEVEL_INFO);
				}
#endif  // USE_DISCOVERY
#ifdef USE_WEBSERVER
				if (Settings.webserver) {
					StartWebserver(Settings.webserver, WiFi.localIP());
#ifdef USE_DISCOVERY
#ifdef WEBSERVER_ADVERTISE
					MDNS.addService("http", "tcp", 80);
#endif  // WEBSERVER_ADVERTISE
#endif  // USE_DISCOVERY
				}
				else {
					StopWebserver();
				}
#endif  // USE_WEBSERVER
			}
			else {
				mdns_begun = false;
			}
		}
	}
}

int WifiState()
{
	int state;

	if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0)) {
		state = WIFI_RESTART;
	}
	if (wifi_config_type) {
		state = wifi_config_type;
	}
	return state;
}

void WifiConnect()
{
	WiFi.persistent(false);   // Solve possible wifi init errors
	wifi_status = 0;
	wifi_retry = WIFI_RETRY_SEC;
	wifi_counter = 1;
}

