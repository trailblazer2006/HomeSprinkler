#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>

#include <ESP8266mDNS.h>               // ArduinoOTA
#include <ArduinoOTA.h>

#include "RtcTime.h"
#include "Wifi.h"

#define project "HomeSprinkler"

// -- Project -------------------------------------
#define PROJECT                "sonoff"          // PROJECT is used as the default topic delimiter and OTA file name
//   As an IDE restriction it needs to be the same as the main .ino file

#define CFG_HOLDER             0x20161209        // [Reset 1] Change this value to load following default configuration parameters
#define SAVE_DATA              1                 // [SaveData] Save changed parameters to Flash (0 = disable, 1 - 3600 seconds)
#define SAVE_STATE             1                 // [SetOption0] Save changed power state to Flash (0 = disable, 1 = enable)

// -- Wifi ----------------------------------------
#define WIFI_IP_ADDRESS        "0.0.0.0"         // [IpAddress1] Set to 0.0.0.0 for using DHCP or IP address
#define WIFI_GATEWAY           "192.168.2.254"   // [IpAddress2] If not using DHCP set Gateway IP address
#define WIFI_SUBNETMASK        "255.255.255.0"   // [IpAddress3] If not using DHCP set Network mask
#define WIFI_DNS               "192.168.2.27"    // [IpAddress4] If not using DHCP set DNS IP address (might be equal to WIFI_GATEWAY)

#define STA_SSID1              ""                // [Ssid1] Wifi SSID
#define STA_PASS1              ""                // [Password1] Wifi password
#define STA_SSID2              ""                // [Ssid2] Optional alternate AP Wifi SSID
#define STA_PASS2              ""                // [Password2] Optional alternate AP Wifi password
#define WIFI_CONFIG_TOOL       WIFI_WPSCONFIG    // [WifiConfig] Default tool if wifi fails to connect
//   (WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG, WIFI_RETRY, WIFI_WAIT)

// -- Syslog --------------------------------------
#define SYS_LOG_HOST           ""                // [LogHost] (Linux) syslog host
#define SYS_LOG_PORT           514               // [LogPort] default syslog UDP port
#define SYS_LOG_LEVEL          LOG_LEVEL_NONE    // [SysLog]
#define SERIAL_LOG_LEVEL       LOG_LEVEL_INFO    // [SerialLog]
#define WEB_LOG_LEVEL          LOG_LEVEL_INFO    // [WebLog]

// -- Ota -----------------------------------------
#define OTA_URL                "http://sonoff.maddox.co.uk/tasmota/sonoff.ino.bin"  // [OtaUrl]

// -- MQTT ----------------------------------------
#define MQTT_USE               1                 // [SetOption3] Select default MQTT use (0 = Off, 1 = On)
// !!! TLS uses a LOT OF MEMORY (20k) so be careful to enable other options at the same time !!!
//#define USE_MQTT_TLS                             // EXPERIMENTAL Use TLS for MQTT connection (+53k code, +15k mem) - Disable by //
//   Needs Fingerprint, TLS Port, UserId and Password
#ifdef USE_MQTT_TLS
//  #define MQTT_HOST            "m20.cloudmqtt.com"  // [MqttHost]
#define MQTT_HOST            ""                   // [MqttHost]
#define MQTT_FINGERPRINT     "A5 02 FF 13 99 9F 8B 39 8E F1 83 4F 11 23 65 0B 32 36 FC 07"  // [MqttFingerprint]
#define MQTT_PORT            20123                // [MqttPort] MQTT TLS port
#define MQTT_USER            "cloudmqttuser"      // [MqttUser] Mandatory user
#define MQTT_PASS            "cloudmqttpassword"  // [MqttPassword] Mandatory password
#else
#define MQTT_HOST            ""                // [MqttHost]
#define MQTT_PORT            1883              // [MqttPort] MQTT port (10123 on CloudMQTT)
#define MQTT_USER            "DVES_USER"       // [MqttUser] Optional user
#define MQTT_PASS            "DVES_PASS"       // [MqttPassword] Optional password
#endif

#define MQTT_BUTTON_RETAIN     0                 // [ButtonRetain] Button may send retain flag (0 = off, 1 = on)
#define MQTT_POWER_RETAIN      0                 // [PowerRetain] Power status message may send retain flag (0 = off, 1 = on)
#define MQTT_SWITCH_RETAIN     0                 // [SwitchRetain] Switch may send retain flag (0 = off, 1 = on)

#define MQTT_STATUS_OFF        "OFF"             // [StateText1] Command or Status result when turned off (needs to be a string like "0" or "Off")
#define MQTT_STATUS_ON         "ON"              // [StateText2] Command or Status result when turned on (needs to be a string like "1" or "On")
#define MQTT_CMND_TOGGLE       "TOGGLE"          // [StateText3] Command to send when toggling (needs to be a string like "2" or "Toggle")
#define MQTT_CMND_HOLD         "HOLD"            // [StateText4] Command to send when button is kept down for over KEY_HOLD_TIME * 0.1 seconds (needs to be a string like "HOLD")

// -- MQTT topics ---------------------------------
//#define MQTT_FULLTOPIC         "tasmota/bedroom/%topic%/%prefix%/" // Up to max 80 characers
#define MQTT_FULLTOPIC         "%prefix%/%topic%/" // [FullTopic] Subscribe and Publish full topic name - Legacy topic

// %prefix% token options
#define SUB_PREFIX             "cmnd"            // [Prefix1] Sonoff devices subscribe to %prefix%/%topic% being SUB_PREFIX/MQTT_TOPIC and SUB_PREFIX/MQTT_GRPTOPIC
#define PUB_PREFIX             "stat"            // [Prefix2] Sonoff devices publish to %prefix%/%topic% being PUB_PREFIX/MQTT_TOPIC
#define PUB_PREFIX2            "tele"            // [Prefix3] Sonoff devices publish telemetry data to %prefix%/%topic% being PUB_PREFIX2/MQTT_TOPIC/UPTIME, POWER and TIME
//   May be named the same as PUB_PREFIX
// %topic% token options (also ButtonTopic and SwitchTopic)
#define MQTT_TOPIC             PROJECT           // [Topic] (unique) MQTT device topic
#define MQTT_GRPTOPIC          "sonoffs"         // [GroupTopic] MQTT Group topic
#define MQTT_CLIENT_ID         "DVES_%06X"       // [MqttClient] Also fall back topic using Chip Id = last 6 characters of MAC address

// -- MQTT - Telemetry ----------------------------
#define TELE_PERIOD            300               // [TelePeriod] Telemetry (0 = disable, 10 - 3600 seconds)

// -- MQTT - Domoticz -----------------------------
#define USE_DOMOTICZ                             // Enable Domoticz (+6k code, +0.3k mem) - Disable by //
#define DOMOTICZ_IN_TOPIC      "domoticz/in"   // Domoticz Input Topic
#define DOMOTICZ_OUT_TOPIC     "domoticz/out"  // Domoticz Output Topic
#define DOMOTICZ_UPDATE_TIMER  0               // [DomoticzUpdateTimer] Send relay status (0 = disable, 1 - 3600 seconds) (Optional)

// -- MQTT - Home Assistant Discovery -------------
#define USE_HOME_ASSISTANT                       // Enable Home Assistant Discovery Support (+1k4 code)
#define HOME_ASSISTANT_DISCOVERY_PREFIX "homeassistant"  // Home Assistant discovery prefix
#define HOME_ASSISTANT_DISCOVERY_ENABLE 0      // [SetOption19] Home Assistant Discovery (0 = Disable, 1 = Enable)

// -- HTTP ----------------------------------------
#define USE_WEBSERVER                            // Enable web server and wifi manager (+66k code, +8k mem) - Disable by //
#define WEB_SERVER           2                 // [WebServer] Web server (0 = Off, 1 = Start as User, 2 = Start as Admin)
#define WEB_PORT             80                // Web server Port for User and Admin mode
#define WEB_USERNAME         "admin"           // Web server Admin mode user name
#define WEB_PASSWORD         ""                // [WebPassword] Web server Admin mode Password for WEB_USERNAME (empty string = Disable)
#define FRIENDLY_NAME        "Sonoff"          // [FriendlyName] Friendlyname up to 32 characters used by webpages and Alexa
#define USE_EMULATION                          // Enable Belkin WeMo and Hue Bridge emulation for Alexa (+16k code, +2k mem)
#define EMULATION          EMUL_NONE         // [Emulation] Select Belkin WeMo (single relay/light) or Hue Bridge emulation (multi relay/light) (EMUL_NONE, EMUL_WEMO or EMUL_HUE)

// -- mDNS ----------------------------------------
#define USE_DISCOVERY                            // Enable mDNS for the following services (+8k code, +0.3k mem) - Disable by //
#define WEBSERVER_ADVERTISE                    // Provide access to webserver by name <Hostname>.local/
#define MQTT_HOST_DISCOVERY                    // Find MQTT host server (overrides MQTT_HOST if found)

// -- Time - Up to three NTP servers in your region
#define NTP_SERVER1            "pool.ntp.org"       // [NtpServer1] Select first NTP server by name or IP address (129.250.35.250)
#define NTP_SERVER2            "nl.pool.ntp.org"    // [NtpServer2] Select second NTP server by name or IP address (5.39.184.5)
#define NTP_SERVER3            "0.nl.pool.ntp.org"  // [NtpServer3] Select third NTP server by name or IP address (93.94.224.67)

// -- Time - Start Daylight Saving Time and timezone offset from UTC in minutes
#define TIME_DST               North, Last, Sun, Mar, 2, +120  // Northern Hemisphere, Last sunday in march at 02:00 +120 minutes

// -- Time - Start Standard Time and timezone offset from UTC in minutes
#define TIME_STD               North, Last, Sun, Oct, 3, +60   // Northern Hemisphere, Last sunday in october 02:00 +60 minutes

// -- Application ---------------------------------
#define APP_TIMEZONE           1                 // [Timezone] +1 hour (Amsterdam) (-12 .. 12 = hours from UTC, 99 = use TIME_DST/TIME_STD)
#define APP_LEDSTATE           LED_POWER         // [LedState] Function of led (LED_OFF, LED_POWER, LED_MQTTSUB, LED_POWER_MQTTSUB, LED_MQTTPUB, LED_POWER_MQTTPUB, LED_MQTT, LED_POWER_MQTT)
#define APP_PULSETIME          0                 // [PulseTime] Time in 0.1 Sec to turn off power for relay 1 (0 = disabled)
#define APP_POWERON_STATE      3                 // [PowerOnState] Power On Relay state (0 = Off, 1 = On, 2 = Toggle Saved state, 3 = Saved state)
#define APP_BLINKTIME          10                // [BlinkTime] Time in 0.1 Sec to blink/toggle power for relay 1
#define APP_BLINKCOUNT         10                // [BlinkCount] Number of blinks (0 = 32000)
#define APP_SLEEP              0                 // [Sleep] Sleep time to lower energy consumption (0 = Off, 1 - 250 mSec)

#define KEY_HOLD_TIME          40                // [SetOption32] Number of 0.1 seconds to hold Button or external Pushbutton before sending HOLD message
#define SWITCH_MODE            TOGGLE            // [SwitchMode] TOGGLE, FOLLOW, FOLLOW_INV, PUSHBUTTON, PUSHBUTTON_INV, PUSHBUTTONHOLD, PUSHBUTTONHOLD_INV or PUSHBUTTON_TOGGLE (the wall switch state)
#define WS2812_LEDS            30                // [Pixels] Number of WS2812 LEDs to start with

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char blynk_auth[] = "3f2069fd89fb45e39f754b284a4ee42a";

WidgetLED led(V0);                                    // BLYNK: LED widget
Ticker flicker;                                       // BLYNK: LED flicker

//  SCL GPIO5
//  SDA GPIO4
#define OLED_RESET 0                                  //  GPIO0
Adafruit_SSD1306 display(OLED_RESET);

ulong loop_timer = 0;                                 // 0.1 sec loop timer
byte loop_count = 0;                                  // loop counter

Ticker blinker;                                       // BOARD: LED blinker
int restart_flag = 0;                                 // Sonoff restart flag
int wifi_state_flag = WIFI_RESTART;                   // Wifi state flag
uint32_t uptime = 0;                                  //  Counting every second until 4294967295 = 130 year

enum modState_t { MODE_OFF, MODE_IDLE, MODE_ACTIVE, MODE_SPRINKLING };

#define PUMP_PIN D5           /* вывод с ШИМ  */      //  объявляем константу с указанием номера вывода, к которому подключен силовой ключ
#define TIME_WAITING 60                               //  объявляем константу для хранения времени ожидания после полива               (в секундах)
uint8_t  arrMoisture[10];                             //  объявляем массив для хранения 10 последних значений влажности почвы
uint16_t valMoisture;                                 //  объявляем переменную для расчёта среднего значения влажности почвы
uint32_t timSprinkling;                               //  объявляем переменную для хранения времени начала последнего полива           (в миллисекундах)
uint32_t timSketch;                                   //  объявляем переменную для хранения времени прошедшего с момента старта скетча (в миллисекундах)
uint8_t  timDuration = 5;     /* по умолчанию */      //  объявляем переменную для хранения длительности полива                        (в секундах)     от 0 до 100
uint8_t  limMoisture = 0;     /* по умолчанию */      //  объявляем переменную для хранения пороговой влажности почвы                  (для вкл насоса) от 0 до 100
uint8_t  modState = MODE_OFF; /* при старте   */      //  объявляем переменную для хранения состояния устройства: 0-не активно, 1-ожидание, 2-активно, 3-полив, 4-установка пороговой влажности, 5-установка времени полива

void setup()
{
	// Initializing serial port for debugging purposes
	Serial.begin(115200);
	delay(10);

	pinMode(PUMP_PIN, OUTPUT);                        //  переводим вывод PUMP_PIN в режим выхода
	digitalWrite(PUMP_PIN, LOW);                      //  выключаем насос
	timSprinkling = 0;                                //  сбрасываем время начала последнего полива

												// by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
												// init done

												// Show image buffer on the display hardware.
												// Since the buffer is intialized with an Adafruit splashscreen
												// internally, this will display the splashscreen.
	display.display();
	delay(1000);

	// Clear the buffer.
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0, 0);

	display.println("Connecting to Wifi..");
	display.display();

	//WiFiManager
	//Local intialization. Once its business is done, there is no need to keep it around
	WiFiManager wifiManager;
	//reset settings - for testing
	//wifiManager.resetSettings();

	//sets timeout until configuration portal gets turned off
	//useful to make it all retry or go to sleep
	//in seconds
	wifiManager.setTimeout(180);

	//fetches ssid and pass and tries to connect
	//if it does not connect it starts an access point with the specified name
	//here  "AutoConnectAP"
	//and goes into a blocking loop awaiting configuration
	if (!wifiManager.autoConnect(project)) {
		Serial.println("failed to connect and hit timeout");
		delay(3000);
		//reset and try again, or maybe put it to deep sleep
		ESP.reset();
		delay(5000);
	}

	//if you get here you have connected to the WiFi
	display.println("connected...yeey :)");
	display.display();

	Blynk.config(blynk_auth);

	//OTA
	ArduinoOTA.onStart([]() {
		Serial.println("Start OTA");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
		delay(5000);
		ESP.restart();
	});
	ArduinoOTA.setHostname(project);
	ArduinoOTA.begin();

	WifiConnect();

	RtcInit();
}

uint16_t getAdc0()
{
	uint16_t alr = 0;
	for (byte i = 0; i < 32; i++) {
		alr += analogRead(A0);
		delay(1);
	}
	return alr >> 5;
}

void every_second()
{
	TIME_T tmpTime;
	uint16_t tmpVal;
	char dt[21];

	uptime++;

	tmpVal = getAdc0();
	valMoisture = 0;
	for (int i = 0; i < 9; i++)
	{
		arrMoisture[i] = arrMoisture[i + 1];
	}
	arrMoisture[9] = map(tmpVal, 0, 1023, 0, 100);
	for (int i = 0; i < 10; i++)
	{
		valMoisture += arrMoisture[i];
	}
	valMoisture /= 10; // вычисляем среднее значение влажности почвы

	Blynk.virtualWrite(1, valMoisture);

	// Clear the buffer.
	display.clearDisplay();
	display.setTextSize(1);
	display.setCursor(0, 0);

	display.setTextColor(BLACK, WHITE);
	display.print(WiFi.status() == WL_CONNECTED ? "i" : "x"); // displaying wifi state
	switch (modState)
	{
	case MODE_OFF:
		display.println("  OFF    ");
		break;
	case MODE_IDLE:
		display.println("  IDLE   ");
		break;
	case MODE_ACTIVE:
		display.println(" ACTIVE  ");
		break;
	case MODE_SPRINKLING:
		display.println("SPRINKLIN"); // "SPRINKLING" is too long (max 11 chars)
		break;
	default:
		break;
	}

	BreakTime(uptime, tmpTime);
	// "P128DT14H35M44S" - ISO8601:2004 - https://en.wikipedia.org/wiki/ISO_8601 Durations
	// snprintf_P(dt, sizeof(dt), PSTR("P%dDT%02dH%02dM%02dS"), ut.days, ut.hour, ut.minute, ut.second);
	// "128 14:35:44" - OpenVMS
	// "128T14:35:44" - Tasmota
	snprintf_P(dt, sizeof(dt), PSTR("%dT%02d:%02d:%02d"), tmpTime.days, tmpTime.hour, tmpTime.minute, tmpTime.second);

	display.setTextColor(WHITE, BLACK);
	display.print("Moist:");
	display.print(valMoisture);
	display.println("%");
	display.print("Durat:");
	display.print(timDuration);
	display.println("s");
	display.print("Limit:");
	display.print(limMoisture);
	display.println("%"); 
	display.println("Uptime:");
	display.println(dt);

	display.display();
}

void loop()
{
	timSketch = millis();                             //  читаем текущее время с момента старта скетча
	if (timSprinkling > timSketch) {
		timSprinkling = 0;                            //  обнуляем время начала последнего полива, если произошло переполнение millis()
	}

	if (millis() > loop_timer)
	{
		// every 0.1 second
		loop_timer = millis() + 100;
		loop_count++;

		if (loop_count == 10)
		{
			// every second
			loop_count = 0;
			every_second();
		}

		/*-------------------------------------------------------------------------------------------*\
		* Every second at 0.2 second interval
		\*-------------------------------------------------------------------------------------------*/

		switch (loop_count) {
		case 2:
			break;
		case 4:
			break;
		case 6:
			WifiCheck(wifi_state_flag);
			wifi_state_flag = WIFI_RESTART;
			break;
		}
	}

	if (WiFi.status() == WL_CONNECTED)
	{
		//ota loop
		ArduinoOTA.handle();

		Blynk.run();
	}
	else {
		WiFiManager.
	}

	//*******Управление устройством:*******
	switch (modState) {
	case MODE_OFF:                                    //  Устройство не активно
		if (timDuration && limMoisture) {             //  если заданы длительность полива и пороговая влажность
			modState = MODE_ACTIVE;
			flicker.detach();
			led.off();
			uptime = 0;
		}
		break;
	case MODE_IDLE:                                   //  Устройство в режиме ожидания (после полива)
		if (timDuration + TIME_WAITING - ((timSketch - timSprinkling) / 1000) <= 0) {
			modState = MODE_ACTIVE;                   //  если закончилось время ожидания
			flicker.detach();
			led.off();
			uptime = 0;
		}
		break;
	case MODE_ACTIVE:                                 //  Устройство активно
		if (!timDuration || !limMoisture) {           //  если не заданы длительность полива или пороговая влажность
			modState = MODE_OFF;
			flicker.detach();
			led.off();
			uptime = 0;
		}
		else if (valMoisture <= limMoisture) {        //  если текущая влажность почвы меньше пороговой
			timSprinkling = timSketch;
			modState = MODE_SPRINKLING;
			analogWrite(PUMP_PIN, 1023);
			flicker.detach();
			led.on();
			uptime = 0;
		}
		break;
	case MODE_SPRINKLING:                             //  Устройство в режиме полива
		if (timDuration - ((timSketch - timSprinkling) / 1000) <= 0) {
			modState = MODE_IDLE;                     //  если закончилось время полива
			analogWrite(PUMP_PIN, 0);
			flicker.attach(0.2, flick);
			uptime = 0;
		}
		break;
	}

	//  yield();     // yield == delay(0), delay contains yield, auto yield in loop
	delay(0);  // https://github.com/esp8266/Arduino/issues/2021
}

void blink() {
	int val = digitalRead(LED_BUILTIN);
	if (val) {
		digitalWrite(LED_BUILTIN, LOW);
	}
	else {
		digitalWrite(LED_BUILTIN, HIGH);
	}
}

void flick() {
	uint8_t val = led.getValue();
	if (val) {
		led.off();
	}
	else {
		led.on();
	}
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
	switch (modState) {
	case MODE_OFF:                                    //  Устройство не активно
		led.off();
		break;
	case MODE_IDLE:                                   //  Устройство в режиме ожидания (после полива)
		break;
	case MODE_ACTIVE:                                 //  Устройство активно
		led.off();
		break;
	case MODE_SPRINKLING:                             //  Устройство в режиме полива
		led.on();
		break;
	}
	// Alternatively, you could override server state using:
	Blynk.virtualWrite(1, valMoisture);
	Blynk.syncVirtual(2, timDuration);
	Blynk.syncVirtual(3, limMoisture);
}

BLYNK_READ(1)
{
	Blynk.virtualWrite(1, valMoisture);
}

BLYNK_READ(2)
{
	Blynk.virtualWrite(2, timDuration);
}

BLYNK_WRITE(2) {
	timDuration = param.asInt();
	if (timDuration < 0) {
		timDuration = 0;
	}
	else if (timDuration > 100) {
		timDuration = 100;
	}
}

BLYNK_READ(3)
{
	Blynk.virtualWrite(3, limMoisture);
}

BLYNK_WRITE(3) {
	limMoisture = param.asInt();
	if (limMoisture < 0) {
		limMoisture = 0;
	}
	else if (limMoisture > 100) {
		limMoisture = 100;
	}
}
