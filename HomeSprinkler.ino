#include "Adafruit_SSD1306.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <BlynkSimpleEsp8266.h>

#define project "HomeSprinkler"

#define SENSORPERIOD 0
#define SENSORVALUE  500   //dry soil

#define PUMP_PIN D5

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char blynk_auth[] = "3f2069fd89fb45e39f754b284a4ee42a";

#include <ESP8266mDNS.h>               // ArduinoOTA
#include <ArduinoOTA.h>

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

ulong loop_timer = 0;                 // 0.1 sec loop timer
byte loop_count = 0;                  // loop counter
int16_t sensor_count = 0;             // sensor counter

int pump = LOW;
int16_t humidity = 0;
int16_t minimal = 50;
uint16_t arrMoisture[10];                             //  объявляем массив для хранения 10 последних значений влажности почвы
uint32_t valMoisture;                                 //  объявляем переменную для расчёта среднего значения влажности почвы
uint16_t limMoisture = 0;    /* по умолчанию */       //  объявляем переменную для хранения пороговой влажности почвы                  (для вкл насоса) от 0 до 999

void setup()
{
	// Initializing serial port for debugging purposes
	Serial.begin(115200);
	delay(10);

	pinMode(PUMP_PIN, OUTPUT);

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

void set_pump(int state)
{
	pump = state ? HIGH : LOW;
	digitalWrite(PUMP_PIN, pump);
	Blynk.virtualWrite(0, pump);
}

void every_second()
{
	sensor_count--;
	if (sensor_count <= 0) {
		uint16_t val = getAdc0();

		valMoisture = 0;
		for (int i = 0; i<9; i++) 
		{
			arrMoisture[i] = arrMoisture[i + 1]; 
		}
		arrMoisture[9] = val;
		for (int i = 0; i<10; i++) 
		{
			valMoisture += arrMoisture[i]; 
		}
		valMoisture /= 10; // вычисляем среднее значение влажности почвы

		Serial.print("A0: ");
		Serial.println(val);
		humidity = map(valMoisture, 0, 1023, 0, 100);

		Blynk.virtualWrite(1, humidity);

		sensor_count = SENSORPERIOD;
	}

	// Clear the buffer.
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0, 0);

	display.println("Humidity");
	display.print(humidity);
	display.println(" %");
	display.println("Minimal");
	display.print(minimal);
	display.println(" %");
	display.println("Pump is ");
	display.setTextColor(BLACK, WHITE);
	display.println(pump ? "ON" : "OFF");
	display.display();
}

void loop()
{
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

		if (humidity < minimal)
		{
			if (!pump)
			{
				set_pump(HIGH);
			}
		}
		else
		{
			if (pump)
			{
				set_pump(LOW);
			}
		}
	}

	if (WiFi.status() == WL_CONNECTED) 
	{
		//ota loop
		ArduinoOTA.handle();

		Blynk.run();
	}

	// if we get a valid byte, read analog ins:
	if (Serial.available() > 0) 
	{
		int n = Serial.read();
		if (n == '?') 
		{
			// выводим температуру (t) и влажность (h) на монитор порта
			Serial.print("Humidity: ");
			Serial.print(humidity);
			Serial.println(" %");
			Serial.print("Pump is ");
			Serial.println(pump ? "ON" : "OFF");
		}
		else if (n == '1') 
		{
			set_pump(HIGH);
		}
		else if (n == '0') 
		{
			set_pump(LOW);
		}
	}

	//  yield();     // yield == delay(0), delay contains yield, auto yield in loop
	delay(0);  // https://github.com/esp8266/Arduino/issues/2021
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
	// Request the latest state from the server
	//Blynk.syncVirtual(V2);

	// Alternatively, you could override server state using:
	Blynk.virtualWrite(0, pump);
	Blynk.virtualWrite(1, humidity);
	Blynk.virtualWrite(2, minimal);
}

BLYNK_READ(0)
{
	Blynk.virtualWrite(0, pump);
}

BLYNK_WRITE(0) {
	int par = param.asInt();
	set_pump(par);
}

BLYNK_READ(1)
{
	Blynk.virtualWrite(1, humidity);
}

BLYNK_READ(2)
{
	Blynk.virtualWrite(2, minimal);
}

BLYNK_WRITE(2) {
	minimal = param.asInt();
}
