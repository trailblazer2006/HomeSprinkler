#pragma once

#define PROJECT       "HomeSprinkler"      // PROJECT is used as the default topic delimiter and OTA file name
#define WIFI_HOSTNAME "%s-%04d"            // Expands to <PROJECT>-<last 4 decimal chars of MAC address>

//  SCL GPIO5
//  SDA GPIO4
#define OLED_RESET    0                    //  GPIO0

#define PUMP_PIN      D5 /* вывод с ШИМ */ //  объявляем константу с указанием номера вывода, к которому подключен силовой ключ

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial


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
} RtcTime;

enum ModStateOptions { MODE_OFF, MODE_IDLE, MODE_ACTIVE, MODE_SPRINKLING };
