//﻿#include <core_version.h>  // Arduino_Esp8266 version information (ARDUINO_ESP8266_RELEASE and ARDUINO_ESP8266_RELEASE_2_3_0)
#include "HomeSprinkler.h" // Enumerations and constants
#include "settings.h"


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>

#include <ESP8266mDNS.h>               // ArduinoOTA
#include <ArduinoOTA.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char blynk_auth[] = "3f2069fd89fb45e39f754b284a4ee42a";

WidgetLED led(V0);                                    // BLYNK: LED widget
Ticker flicker;                                       // BLYNK: LED flicker

//  SCL GPIO5
//  SDA GPIO4
#define OLED_RESET 0                                  //  GPIO0
Adafruit_SSD1306 display(OLED_RESET);

char my_hostname[33];                       // Composed Wifi hostname

ulong loop_timer = 0;                       // 0.1 sec loop timer
byte loop_count = 0;                        // loop counter

Ticker blinker;                             // BOARD: LED blinker

int restart_flag = 0;                       // Sonoff restart flag
uint32_t uptime = 0;                        // Counting every second until 4294967295 = 130 year


#define PUMP_PIN D5           /* вывод с ШИМ  */      //  объявляем константу с указанием номера вывода, к которому подключен силовой ключ
uint8_t  arrMoisture[10];                             //  объявляем массив для хранения 10 последних значений влажности почвы
uint16_t valMoisture;                                 //  объявляем переменную для расчёта среднего значения влажности почвы
uint32_t timSprinkling;                               //  объявляем переменную для хранения времени начала последнего полива           (в миллисекундах)
uint32_t timSketch;                                   //  объявляем переменную для хранения времени прошедшего с момента старта скетча (в миллисекундах)
uint8_t  timDuration = 30;    /* по умолчанию */      //  объявляем переменную для хранения длительности полива                        (в секундах)     от 0 до 100
uint8_t  timWaiting = 1;      /* по умолчанию */      //  объявляем переменную для хранения времени ожидания после полива              (в минутах)      от 0 до 100
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

  snprintf_P(my_hostname, sizeof(my_hostname) - 1, WIFI_HOSTNAME, PROJECT, ESP.getChipId() & 0x1FFF);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();

  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect(my_hostname);

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

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
  ArduinoOTA.setHostname(my_hostname);
  ArduinoOTA.begin();

  RtcInit();
}

String getStateText(int pad = 0) {
  String str;
  char buffer[11];

  switch (modState)
  {
    case MODE_OFF:
      str = String("OFF");
      break;
    case MODE_IDLE:
      str = String("IDLE");
      break;
    case MODE_ACTIVE:
      str = String("ACTIVE");
      break;
    case MODE_SPRINKLING:
      str = String("SPRINKLING");
      break;
    default:
      str = String("");
      break;
  }

  if (pad) {
    int length = str.length();
    if (pad < length) {
      str = str.substring(0, pad - 1);
    }
    else if (pad > length) {
      int start = (pad - length) / 2;

      memset(&buffer, ' ', pad);
      memcpy(&buffer[start], str.begin(), length);
      buffer[pad] = 0;

      str = String(buffer);
    }
  }

  return str;
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
  display.println(getStateText(10));

  BreakTime(uptime, tmpTime);
  // "P128DT14H35M44S" - ISO8601:2004 - https://en.wikipedia.org/wiki/ISO_8601 Durations
  snprintf_P(dt, sizeof(dt), PSTR("%dT%02d:%02d:%02d"), tmpTime.days, tmpTime.hour, tmpTime.minute, tmpTime.second);

  display.setTextColor(WHITE, BLACK);
//  display.print("Moist:");
//  display.print(valMoisture);
//  display.println("%");
//  display.print("Limit:");
//  display.print(limMoisture);
//  display.println("%");
  display.print("Durat:");
  display.print(timDuration);
  display.println("s");
  display.print("Idle: ");
  display.print(timDuration + (timWaiting * 60) - ((timSketch - timSprinkling) / 1000));
  display.println("s");
  display.println("Uptime:");
  display.println(dt);

  display.display();
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    //ota loop
    ArduinoOTA.handle();

    Blynk.run();
  }

  timSketch = millis();                           //  читаем текущее время с момента старта скетча
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
      Every second at 0.2 second interval
    \*-------------------------------------------------------------------------------------------*/

    switch (loop_count) {
      case 2:
        break;
      case 4:
        if (restart_flag) {
          restart_flag--;
          if (restart_flag <= 0) {
            ESP.restart();
          }
        }
        break;
    }
  }

  //*******Управление устройством:*******
  switch (modState) {
    case MODE_OFF:                                  //  Устройство не активно
      if (timDuration && limMoisture) {             //  если заданы длительность полива и пороговая влажность
        modState = MODE_ACTIVE;
        flicker.detach();
        led.off();
        uptime = 0;
      }
      break;
    case MODE_IDLE:                                 //  Устройство в режиме ожидания (после полива)
      if (timDuration + (timWaiting * 60) - ((timSketch - timSprinkling) / 1000) <= 0) {
        modState = MODE_ACTIVE;                     //  если закончилось время ожидания
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

BLYNK_READ(4)
{
  Blynk.virtualWrite(4, timDuration + (timWaiting * 60) - ((timSketch - timSprinkling) / 1000));
}
