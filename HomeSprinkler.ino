#include "HomeSprinkler.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <BlynkSimpleEsp8266.h>

#include <ESP8266mDNS.h>                              // ArduinoOTA
#include <ArduinoOTA.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char blynk_auth[] = "3f2069fd89fb45e39f754b284a4ee42a";

Adafruit_SSD1306 display(OLED_RESET);

char my_hostname[33];                                      //  composed Wifi hostname
ulong loop_timer = 0;                                      //  0.1 sec loop timer
byte loop_count = 0;                                       //  loop counter

uint8_t  arrMoisture[10];                                  //  объявляем массив для хранения 10 последних значений влажности почвы
uint16_t valMoisture;                                      //  объявляем переменную для расчёта среднего значения влажности почвы
uint8_t  modState = MODE_OFF;        /* statup value  */   //  объявляем переменную для хранения состояния устройства: 0-не активно, 1-ожидание, 2-активно, 3-полив
uint32_t timIdle;                                          //  объявляем переменную для хранения времени ожидания                 (в секундах)
uint32_t timSprinkling;                                    //  объявляем переменную для хранения времени полива                   (в секундах)
uint8_t  timDuration = 10;           /* default value */   //  объявляем переменную для хранения длительности полива              (в секундах)     от 0 до 100
uint32_t timWaiting = 60 * 24 * 3;   /* default value */   //  объявляем переменную для хранения времени ожидания после полива    (в минутах)
uint8_t  limMoisture = 0;            /* default value */   //  объявляем переменную для хранения пороговой влажности почвы        (для вкл насоса) от 0 до 100

void setup()
{
  // Initializing serial port for debugging purposes
  Serial.begin(115200);
  delay(10);

  pinMode(PUMP_PIN, OUTPUT);                        //  переводим вывод PUMP_PIN в режим выхода
  digitalWrite(PUMP_PIN, LOW);                      //  выключаем насос

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

String getStateText(int pad = 0)
{
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

  if (pad)
  {
    int length = str.length();
    if (pad < length)
    {
      str = str.substring(0, pad - 1);
    }
    else if (pad > length)
    {
      int start = (pad - length) / 2;

      memset(&buffer, ' ', pad);
      memcpy(&buffer[start], str.begin(), length);
      buffer[pad] = 0;

      str = String(buffer);
    }
  }

  return str;
}

String getTimeLeft()
{
  TIME_T tmpTime;
  char dt[21];

  int32_t timLeft = (timWaiting * 60) - timIdle;
  if (timLeft < 0) timLeft = 0;

  BreakTime(timLeft, tmpTime);
  // "P128DT14H35M44S" - ISO8601:2004 - https://en.wikipedia.org/wiki/ISO_8601 Durations
  snprintf_P(dt, sizeof(dt), PSTR("%dT%02d:%02d:%02d"), tmpTime.days, tmpTime.hour, tmpTime.minute, tmpTime.second);

  return dt;
}

void every_second()
{
  uint16_t tmpVal;
  TIME_T tmpTime;
  char dt[21];

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

  if (modState == MODE_IDLE)
  {
    timIdle++; // increase idle time
  }
  else if (modState == MODE_SPRINKLING)
  {
    timSprinkling++; // increase sprinkling time
  }

  // Clear the buffer.
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.setTextColor(BLACK, WHITE);
  display.println(getStateText(10));

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
  display.println("Idle:");
  display.println(getTimeLeft());

  display.display();
}

void setModeOff()
{
  modState = MODE_OFF;
  analogWrite(PUMP_PIN, LOW);
  Blynk.virtualWrite(0, getStateText());
}

void setModeIdle()
{
  timIdle = 0;
  modState = MODE_IDLE;
  analogWrite(PUMP_PIN, LOW);
  Blynk.virtualWrite(0, getStateText());
}

void setModeActive()
{
  modState = MODE_ACTIVE;
  analogWrite(PUMP_PIN, LOW);
  Blynk.virtualWrite(0, getStateText());
}

void setModeSprinkling()
{
  timSprinkling = 0;
  modState = MODE_SPRINKLING;
  analogWrite(PUMP_PIN, HIGH);
  Blynk.virtualWrite(0, getStateText());
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    //ota loop
    ArduinoOTA.handle();

    Blynk.run();
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

    //*******Управление устройством:*******
    switch (modState)
    {
      case MODE_OFF:                          //  Устройство не активно
        if (timDuration && limMoisture)       //  если заданы длительность полива и пороговая влажность
        {
          setModeIdle();
        }
        break;
      case MODE_IDLE:                          //  Устройство в режиме ожидания (после полива)
        if ((timWaiting * 60) <= timIdle)      //  если закончилось время ожидания
        {
          setModeActive();
        }
        break;
      case MODE_ACTIVE:                        //  Устройство активно
        if (!timDuration || !limMoisture)      //  если не заданы длительность полива или пороговая влажность
        {
          setModeOff();
        }
        else if (valMoisture <= limMoisture)   //  если текущая влажность почвы меньше пороговой
        {
          setModeSprinkling();
        }
        break;
      case MODE_SPRINKLING:                    //  Устройство в режиме полива
        if (timDuration <= timSprinkling)      //  если закончилось время полива
        {
          setModeIdle();
        }
        break;
    }

  }

  //  yield();     // yield == delay(0), delay contains yield, auto yield in loop
  delay(0);  // https://github.com/esp8266/Arduino/issues/2021
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
  // Alternatively, you could override server state using:
  Blynk.virtualWrite(0, getStateText());
  Blynk.virtualWrite(1, valMoisture);
  Blynk.syncVirtual(2, timDuration);
  Blynk.syncVirtual(3, limMoisture);
  Blynk.virtualWrite(4, getTimeLeft());
  Blynk.syncVirtual(5, timWaiting);
}

BLYNK_READ(0)
{
  Blynk.virtualWrite(0, getStateText());
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
  Blynk.virtualWrite(4, getTimeLeft());
}

BLYNK_READ(5)
{
  Blynk.virtualWrite(5, timWaiting);
}

BLYNK_WRITE(5) {
  timWaiting = param.asInt();
  if (timWaiting < 1) {
    timWaiting = 1;
  }
}
