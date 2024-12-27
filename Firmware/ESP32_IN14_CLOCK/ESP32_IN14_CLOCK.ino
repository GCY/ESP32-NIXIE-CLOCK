/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/

#include <Wire.h>
#include "ClosedCube_HDC1080.h"
#include "RTClib.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

ClosedCube_HDC1080 hdc1080;

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

//int tubes[] = {25,26,27,14,12,13};
int tubes[] = { 13, 12, 14, 27, 26, 25 };
int K1551[] = { 33, 32, 23, 4 };
int dot = 15;

bool dot_on = false;

int h = 0;
int m = 0;
int s = 0;

float temperature = 0;
float humidity = 0;
const bool dew_point = true;

const bool sync_time = false;

unsigned long dot_last_time = 0;

static char mode = '6';
const unsigned long s1 = 10000;
const unsigned long s2 = 12000;
const unsigned long s3 = 14000;
unsigned long sr = 0;
unsigned short state = 0;

unsigned long sn = 0;
unsigned long snt = 0;

const static int set_time_length = 15;
char set_time[set_time_length];
char set_time_index = 0;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
        mode = rxValue[i];
      }

      for (int i = 0; i < rxValue.length(); i++) {
        char temp = rxValue[i];

        if (set_time_index < set_time_length) {
          set_time[set_time_index++] = temp;
        }

        if (set_time_index == set_time_length) {
          set_time_index = 0;
          if (set_time[0] == 's' || set_time[0] == 'S') {

            boolean set_time_flag = true;

            for (int i = 1; i < set_time_length; ++i) {
              if (set_time[i] >= '0' && set_time[i] <= '9') {
              } else {
                set_time_flag = false;
              }
            }

            if (set_time_flag) {
              char year_char[5];
              char month_char[3];
              char day_char[3];
              char hour_char[3];
              char minute_char[3];
              char second_char[3];

              year_char[0] = set_time[1];
              year_char[1] = set_time[2];
              year_char[2] = set_time[3];
              year_char[3] = set_time[4];

              month_char[0] = set_time[5];
              month_char[1] = set_time[6];

              day_char[0] = set_time[7];
              day_char[1] = set_time[8];

              hour_char[0] = set_time[9];
              hour_char[1] = set_time[10];

              minute_char[0] = set_time[11];
              minute_char[1] = set_time[12];

              second_char[0] = set_time[13];
              second_char[1] = set_time[14];

              int set_year = atoi(year_char);
              int set_month = atoi(month_char);
              int set_day = atoi(day_char);
              int set_hour = atoi(hour_char);
              int set_minute = atoi(minute_char);
              int set_second = atoi(second_char);

              if (set_year < 1970) {
                set_time_flag = false;
              }
              if (set_month < 1 || set_month > 12) {
                set_time_flag = false;
              }
              if (set_day < 1 || set_day > 31) {
                set_time_flag = false;
              }
              if (set_hour < 0 || set_hour > 23) {
                set_time_flag = false;
              }
              if (set_minute < 0 || set_minute > 59) {
                set_time_flag = false;
              }
              if (set_second < 0 || set_second > 59) {
                set_time_flag = false;
              }

              if (set_time_flag) {

                DateTime dt(set_year, set_month, set_day, set_hour, set_minute, set_second);
                rtc.adjust(dt);

                mode = '6';
              }
            }
          }
        }
      }

      Serial.println();
      Serial.println("*********");
    }
  }
};

float HDC1080DewPoint(float temperature, float humidity) {
  return (243.12 * (log(humidity / 100.0) + ((17.62 * temperature) / (243.12 + temperature)))) / (17.62 - (log(humidity / 100.0) + ((17.62 * temperature) / (243.12 + temperature))));
}

void UpdateHDC1080Temperature() {
  static int c = 0;
  static float temperature_t = 0;
  static const float ma = 1.0f;
  if (c < ma) {
    temperature_t += hdc1080.readTemperature();
    ++c;
  } else if (c >= ma) {
    temperature = temperature_t / ma;

    if (dew_point) {
      temperature = HDC1080DewPoint(temperature, humidity);
    }
    temperature_t = 0;
    c = 0;
  }
}

void UpdateHDC1080Humidity() {
  static int c = 0;
  static float humidity_t = 0;
  static const float ma = 1.0f;
  if (c < ma) {
    humidity_t += hdc1080.readHumidity();
    ++c;
  } else if (c >= ma) {
    humidity = humidity_t / ma;
    humidity_t = 0;
    c = 0;
  }
}

int value_mirror(int val) {
  if (val == 0) return 1;
  if (val == 1) return 0;
  if (val == 2) return 9;
  if (val == 3) return 8;
  if (val == 4) return 7;
  if (val == 5) return 6;
  if (val == 6) return 5;
  if (val == 7) return 4;
  if (val == 8) return 3;
  if (val == 9) return 2;
}

void BCD(int val) {
  val = value_mirror(val);

  if (val & 0x01) {
    digitalWrite(K1551[3], HIGH);
  } else {
    digitalWrite(K1551[3], LOW);
  }
  if (val & 0x02) {
    digitalWrite(K1551[2], HIGH);
  } else {
    digitalWrite(K1551[2], LOW);
  }
  if (val & 0x04) {
    digitalWrite(K1551[1], HIGH);
  } else {
    digitalWrite(K1551[1], LOW);
  }
  if (val & 0x04) {
    digitalWrite(K1551[1], HIGH);
  } else {
    digitalWrite(K1551[1], LOW);
  }
  if (val & 0x08) {
    digitalWrite(K1551[0], HIGH);
  } else {
    digitalWrite(K1551[0], LOW);
  }
}

void DisplayTime() {

  DateTime now = rtc.now();
  h = now.hour();
  m = now.minute();
  s = now.second();

  static const int dt1 = 650;
  static const int dt2 = 800;

  digitalWrite(tubes[5], LOW);
  delayMicroseconds(dt1);

  BCD(h / 10);
  digitalWrite(tubes[0], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[0], LOW);
  delayMicroseconds(dt1);

  BCD(h % 10);
  digitalWrite(tubes[1], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[1], LOW);
  delayMicroseconds(dt1);

  BCD(m / 10);
  if (dot_on) digitalWrite(dot, HIGH);
  digitalWrite(tubes[2], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(dot, LOW);
  digitalWrite(tubes[2], LOW);
  delayMicroseconds(dt1);

  BCD(m % 10);
  digitalWrite(tubes[3], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[3], LOW);
  delayMicroseconds(dt1);

  BCD(s / 10);
  if (dot_on) digitalWrite(dot, HIGH);
  digitalWrite(tubes[4], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(dot, LOW);
  digitalWrite(tubes[4], LOW);
  delayMicroseconds(dt1);

  BCD(s % 10);
  digitalWrite(tubes[5], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[5], LOW);
}

void DisplayTemperature() {
  static const int dt1 = 650;
  static const int dt2 = 800;

  digitalWrite(tubes[5], LOW);
  digitalWrite(dot, LOW);
  delayMicroseconds(dt1);
  BCD(((int)temperature) / 10);
  digitalWrite(tubes[2], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[2], LOW);
  delayMicroseconds(dt1);
  BCD(((int)temperature) % 10);
  digitalWrite(tubes[3], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[3], LOW);
  delayMicroseconds(dt1);

  BCD((int)((temperature - ((int)temperature)) * 100) / 10);

  digitalWrite(dot, HIGH);

  digitalWrite(tubes[4], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(dot, LOW);
  digitalWrite(tubes[4], LOW);
  delayMicroseconds(dt1);
  BCD((int)((temperature - ((int)temperature)) * 100) % 10);
  digitalWrite(tubes[5], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[5], LOW);
}

void DisplayHumidity() {
  static const int dt1 = 650;
  static const int dt2 = 800;

  digitalWrite(dot, LOW);
  digitalWrite(tubes[5], LOW);
  delayMicroseconds(dt1);
  BCD(((int)humidity) / 10);
  digitalWrite(tubes[2], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[2], LOW);
  delayMicroseconds(dt1);
  BCD(((int)humidity) % 10);
  digitalWrite(tubes[3], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[3], LOW);
  delayMicroseconds(dt1);

  BCD((int)((humidity - ((int)humidity)) * 100) / 10);

  digitalWrite(dot, HIGH);
  
  digitalWrite(tubes[4], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(dot, LOW);
  digitalWrite(tubes[4], LOW);
  delayMicroseconds(dt1);
  BCD((int)((humidity - ((int)humidity)) * 100) % 10);
  digitalWrite(tubes[5], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[5], LOW);
}

void DisplayRandom() {
  static const int dt1 = 500;
  static const int dt2 = 13000;

  digitalWrite(tubes[5], LOW);
  delayMicroseconds(dt1);
  BCD(random(0, 9));
  digitalWrite(tubes[0], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[0], LOW);
  delayMicroseconds(dt1);
  BCD(random(0, 9));
  digitalWrite(tubes[1], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[1], LOW);
  delayMicroseconds(dt1);
  BCD(random(0, 9));
  digitalWrite(tubes[2], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[2], LOW);
  delayMicroseconds(dt1);
  BCD(random(0, 9));
  digitalWrite(tubes[3], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[3], LOW);
  delayMicroseconds(dt1);
  BCD(random(0, 9));
  digitalWrite(tubes[4], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[4], LOW);
  delayMicroseconds(dt1);
  BCD(random(0, 9));
  digitalWrite(tubes[5], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[5], LOW);
}

void DisplaySN() {
  static const int dt1 = 650;
  static const int dt2 = 800;

  digitalWrite(tubes[5], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[0], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[0], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[1], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[1], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[2], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[2], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[3], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[3], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[4], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[4], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[5], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[5], LOW);
}

void DisplaySN_Single() {
  static const int dt1 = 650;
  static const int dt2 = 1000000;

  digitalWrite(tubes[5], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[0], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[0], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[1], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[1], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[2], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[2], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[3], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[3], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[4], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[4], LOW);
  delayMicroseconds(dt1);
  BCD(sn);
  digitalWrite(tubes[5], HIGH);
  delayMicroseconds(dt2);
  digitalWrite(tubes[5], LOW);
}

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("ESP32 IN-14 Clock");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  hdc1080.begin(0x40);
  Serial.print("Manufacturer ID=0x");
  Serial.println(hdc1080.readManufacturerId(), HEX);  // 0x5449 ID of Texas Instruments
  Serial.print("Device ID=0x");
  Serial.println(hdc1080.readDeviceId(), HEX);  // 0x1050 ID of the device

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  if (sync_time) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  for (int i = 0; i < (sizeof(tubes) / sizeof(tubes[0])); ++i) {
    pinMode(tubes[i], OUTPUT);
    digitalWrite(tubes[i], LOW);
  }
  for (int i = 0; i < (sizeof(K1551) / sizeof(K1551[0])); ++i) {
    pinMode(K1551[i], OUTPUT);
    digitalWrite(K1551[i], LOW);
  }
  pinMode(dot, OUTPUT);
  digitalWrite(dot, LOW);
}

void loop() {

  if (deviceConnected) {
    pTxCharacteristic->setValue(&txValue, 1);
    pTxCharacteristic->notify();
    txValue++;
    delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  if (mode == '1') {
    DisplayTime();
  } else if (mode == '2') {
    UpdateHDC1080Temperature();
    DisplayTemperature();
  } else if (mode == '3') {
    UpdateHDC1080Humidity();
    DisplayHumidity();
  } else if (mode == '4') {
    DisplayRandom();
  } else if (mode == '5') {
    if ((millis() - snt) > 1000) {
      snt = millis();
      ++sn;
      if (sn > 9) {
        sn = 0;
      }
    }
    DisplaySN();
  } else if (mode == '6') {
    unsigned long srt = millis() - sr;
    if (srt < s1) {
      if (state != 0) {
        state = 0;
      }
      DisplayTime();
    } else if (srt > s1 && srt < s2) {
      if (state != 1) {
        UpdateHDC1080Temperature();
        state = 1;
      }
      DisplayTemperature();
    } else if (srt > s2 && srt < s3) {
      if (state != 2) {
        UpdateHDC1080Humidity();
        state = 2;
      }
      DisplayHumidity();
    }

    if (srt > s3) {
      sr = millis();
    }
  } else if (mode == '7') {
    DisplaySN_Single();
    ++sn;
    if (sn > 9) {
      sn = 0;
    }
  }

  if ((millis() - dot_last_time) > 1000) {
    dot_on ^= true;
    dot_last_time = millis();
  }
}
