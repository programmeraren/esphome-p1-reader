//-------------------------------------------------------------------------------------
// ESPHome P1 Electricity Meter custom sensor
// Copyright 2023 Hazze Molin
// 
// History
//  0.1.0 2020-11-05:   Initial release (Pär Svanström)
//  1.0.0 2022-12-01:   Forked Pär's release. (Hazze)
//  1.1.0 2022-12-05:   Modifications for RTS, timeout and led receive data. (Hazze)
//  1.5.0 2023-04-01:   Modifications for Vemos D1 Mini and improved led indicators. (Hazze)
//  1.7.0 2023-06-28:   Hardware board rev 1.7 was finalized. Some code improvements. (Hazze)
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
//-------------------------------------------------------------------------------------

#include "esphome.h"

#define BUF_SIZE 64
#define READ_CHARS_MAX  30

class ParsedMessage {
  public:
    double cumulativeActiveImport;
    double cumulativeActiveExport;

    double cumulativeReactiveImport;
    double cumulativeReactiveExport;

    double momentaryActiveImport;
    double momentaryActiveExport;

    double momentaryReactiveImport;
    double momentaryReactiveExport;

    double momentaryActiveImportL1;
    double momentaryActiveExportL1;

    double momentaryActiveImportL2;
    double momentaryActiveExportL2;

    double momentaryActiveImportL3;
    double momentaryActiveExportL3;

    double momentaryReactiveImportL1;
    double momentaryReactiveExportL1;

    double momentaryReactiveImportL2;
    double momentaryReactiveExportL2;

    double momentaryReactiveImportL3;
    double momentaryReactiveExportL3;

    double voltageL1;
    double voltageL2;
    double voltageL3;

    double currentL1;
    double currentL2;
    double currentL3;

    bool crcOk = false;
};

class P1Reader : public Component, public UARTDevice {
  const char* DELIMITERS = "()*:";
  const char* DATA_ID = "1-0";
  char readBuffer[BUF_SIZE+1];

  public:
    Sensor *cumulativeActiveImport = new Sensor();
    Sensor *cumulativeActiveExport = new Sensor();

    Sensor *cumulativeReactiveImport = new Sensor();
    Sensor *cumulativeReactiveExport = new Sensor();

    Sensor *momentaryActiveImport = new Sensor();
    Sensor *momentaryActiveExport = new Sensor();

    Sensor *momentaryReactiveImport = new Sensor();
    Sensor *momentaryReactiveExport = new Sensor();

    Sensor *momentaryActiveImportL1 = new Sensor();
    Sensor *momentaryActiveExportL1 = new Sensor();

    Sensor *momentaryActiveImportL2 = new Sensor();
    Sensor *momentaryActiveExportL2 = new Sensor();

    Sensor *momentaryActiveImportL3 = new Sensor();
    Sensor *momentaryActiveExportL3 = new Sensor();

    Sensor *momentaryReactiveImportL1 = new Sensor();
    Sensor *momentaryReactiveExportL1 = new Sensor();

    Sensor *momentaryReactiveImportL2 = new Sensor();
    Sensor *momentaryReactiveExportL2 = new Sensor();

    Sensor *momentaryReactiveImportL3 = new Sensor();
    Sensor *momentaryReactiveExportL3 = new Sensor();

    Sensor *voltageL1 = new Sensor();
    Sensor *voltageL2 = new Sensor();
    Sensor *voltageL3 = new Sensor();

    Sensor *currentL1 = new Sensor();
    Sensor *currentL2 = new Sensor();
    Sensor *currentL3 = new Sensor();

    P1Reader(UARTComponent *parent) : UARTDevice(parent) {}

    // Pins defined for Vemos D1 Mini.
    #define LED_ONBOARD_PIN 2 // GPIO2 D4
    #define LED_STATUS_PIN 12 // GPIO12 D6
    #define LED_RTS_PIN 15    // GPIO15 D8

    // Delay settings
    #define ONBOARD_FLASH_DELAY 750
    #define STATUS_FLASH_DELAY 950
    #define RTS_DELAY 15000

    boolean onboardFlash = false;
    boolean statusFlash = false;
    unsigned long lastOnboardFlashTime = 0l;
    unsigned long lastStatusFlashTime = 0l;
    unsigned long lastRtsTime = 0l;

    void setup() override {
      ESP_LOGI("exe", " >> SETUP...");
      digitalWrite(LED_RTS_PIN, LOW);
      pinMode(LED_RTS_PIN, OUTPUT);

      digitalWrite(LED_STATUS_PIN, LOW);
      pinMode(LED_STATUS_PIN, OUTPUT);

      digitalWrite(LED_ONBOARD_PIN, LOW);
      pinMode(LED_ONBOARD_PIN, OUTPUT);

      lastOnboardFlashTime = millis();
      lastStatusFlashTime = millis();
      lastRtsTime = millis();
    }

    void loop() override {
      readP1Message();
      onboardLedFlashWithDelay();
      statusLedFlashWithDelay();
      checkTimeToTurnOnRts();
    }

  private:

    void statusLedFlashWithDelay() {
      if (millis() - lastStatusFlashTime > STATUS_FLASH_DELAY) {
        statusLedToggle();
      }
    }

    void statusLedToggle() {
        statusFlash = !statusFlash;
        statusLedOutput();
    }

    void statusLedTurnOn() {
        statusFlash = true;
        statusLedOutput();
    }

    void statusLedTurnOff() {
        statusFlash = false;
        statusLedOutput();
    }

    void statusLedOutput() {
        digitalWrite(LED_STATUS_PIN, statusFlash ? HIGH : LOW);
        lastStatusFlashTime = millis();
    }

    void onboardLedFlashWithDelay() {
      if (millis() - lastOnboardFlashTime > ONBOARD_FLASH_DELAY) {
        onboardFlash = !onboardFlash;
        digitalWrite(LED_ONBOARD_PIN, onboardFlash ? HIGH : LOW);
        lastOnboardFlashTime = millis();
      }
    }

    void checkTimeToTurnOnRts() {
      if (millis() - lastRtsTime > RTS_DELAY) {
        turnOnRts();
        // ESP_LOGI("exe", " > Turned On RTS.");
      }
    }

    void turnOnRts() {
      lastRtsTime = millis();
      digitalWrite(LED_RTS_PIN, HIGH);
    }

    void turnOffRts() {
      digitalWrite(LED_RTS_PIN, LOW);
    }

    uint16_t crc16_update(uint16_t crc, uint8_t a) {
      int i;
      crc ^= a;
      for (i = 0; i < 8; i++) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc = (crc >> 1);
        }
      }
      return crc;
    }

    void parseRow(ParsedMessage* parsed, char* obisCode, char* value) {
      if (strncmp(obisCode, "1.8.0", 5) == 0) {
        parsed->cumulativeActiveImport = atof(value);

      } else if (strncmp(obisCode, "2.8.0", 5) == 0) {
        parsed->cumulativeActiveExport = atof(value);

      } else if (strncmp(obisCode, "3.8.0", 5) == 0) {
        parsed->cumulativeReactiveImport = atof(value);

      } else if (strncmp(obisCode, "4.8.0", 5) == 0) {
        parsed->cumulativeReactiveExport = atof(value);

      } else if (strncmp(obisCode, "1.7.0", 5) == 0) {
        parsed->momentaryActiveImport = atof(value);

      } else if (strncmp(obisCode, "2.7.0", 5) == 0) {
        parsed->momentaryActiveExport = atof(value);

      } else if (strncmp(obisCode, "3.7.0", 5) == 0) {
        parsed->momentaryReactiveImport = atof(value);

      } else if (strncmp(obisCode, "4.7.0", 5) == 0) {
        parsed->momentaryReactiveExport = atof(value);

      } else if (strncmp(obisCode, "21.7.0", 6) == 0) {
        parsed->momentaryActiveImportL1 = atof(value);

      } else if (strncmp(obisCode, "22.7.0", 6) == 0) {
        parsed->momentaryActiveExportL1 = atof(value);

      } else if (strncmp(obisCode, "41.7.0", 6) == 0) {
        parsed->momentaryActiveImportL2 = atof(value);

      } else if (strncmp(obisCode, "42.7.0", 6) == 0) {
        parsed->momentaryActiveExportL2 = atof(value);

      } else if (strncmp(obisCode, "61.7.0", 6) == 0) {
        parsed->momentaryActiveImportL3 = atof(value);

      } else if (strncmp(obisCode, "62.7.0", 6) == 0) {
        parsed->momentaryActiveExportL3 = atof(value);

      } else if (strncmp(obisCode, "23.7.0", 6) == 0) {
        parsed->momentaryReactiveImportL1 = atof(value);

      } else if (strncmp(obisCode, "24.7.0", 6) == 0) {
        parsed->momentaryReactiveExportL1 = atof(value);

      } else if (strncmp(obisCode, "43.7.0", 6) == 0) {
        parsed->momentaryReactiveImportL2 = atof(value);

      } else if (strncmp(obisCode, "44.7.0", 6) == 0) {
        parsed->momentaryReactiveExportL2 = atof(value);

      } else if (strncmp(obisCode, "63.7.0", 6) == 0) {
        parsed->momentaryReactiveImportL3 = atof(value);

      } else if (strncmp(obisCode, "64.7.0", 6) == 0) {
        parsed->momentaryReactiveExportL3 = atof(value);

      } else if (strncmp(obisCode, "32.7.0", 6) == 0) {
        parsed->voltageL1 = atof(value);

      } else if (strncmp(obisCode, "52.7.0", 6) == 0) {
        parsed->voltageL2 = atof(value);

      } else if (strncmp(obisCode, "72.7.0", 6) == 0) {
        parsed->voltageL3 = atof(value);

      } else if (strncmp(obisCode, "31.7.0", 6) == 0) {
        parsed->currentL1 = atof(value);

      } else if (strncmp(obisCode, "51.7.0", 6) == 0) {
        parsed->currentL2 = atof(value);

      } else if (strncmp(obisCode, "71.7.0", 6) == 0) {
        parsed->currentL3 = atof(value);
      }
    }

    void publishSensors(ParsedMessage* parsed) {
      cumulativeActiveImport->publish_state(parsed->cumulativeActiveImport);
      cumulativeActiveExport->publish_state(parsed->cumulativeActiveExport);

      cumulativeReactiveImport->publish_state(parsed->cumulativeReactiveImport);
      cumulativeReactiveExport->publish_state(parsed->cumulativeReactiveExport);

      momentaryActiveImport->publish_state(parsed->momentaryActiveImport);
      momentaryActiveExport->publish_state(parsed->momentaryActiveExport);

      momentaryReactiveImport->publish_state(parsed->momentaryReactiveImport);
      momentaryReactiveExport->publish_state(parsed->momentaryReactiveExport);

      momentaryActiveImportL1->publish_state(parsed->momentaryActiveImportL1);
      momentaryActiveExportL1->publish_state(parsed->momentaryActiveExportL1);

      momentaryActiveImportL2->publish_state(parsed->momentaryActiveImportL2);
      momentaryActiveExportL2->publish_state(parsed->momentaryActiveExportL2);

      momentaryActiveImportL3->publish_state(parsed->momentaryActiveImportL3);
      momentaryActiveExportL3->publish_state(parsed->momentaryActiveExportL3);

      momentaryReactiveImportL1->publish_state(parsed->momentaryReactiveImportL1);
      momentaryReactiveExportL1->publish_state(parsed->momentaryReactiveExportL1);

      momentaryReactiveImportL2->publish_state(parsed->momentaryReactiveImportL2);
      momentaryReactiveExportL2->publish_state(parsed->momentaryReactiveExportL2);

      momentaryReactiveImportL3->publish_state(parsed->momentaryReactiveImportL3);
      momentaryReactiveExportL3->publish_state(parsed->momentaryReactiveExportL3);

      voltageL1->publish_state(parsed->voltageL1);
      voltageL2->publish_state(parsed->voltageL2);
      voltageL3->publish_state(parsed->voltageL3);

      currentL1->publish_state(parsed->currentL1);
      currentL2->publish_state(parsed->currentL2);
      currentL3->publish_state(parsed->currentL3);
    }

    // Non-blocking readline!
    int readBufferedLineNonBlocking(char *buffer, int bufferSizeMax)
    {
      static int readlinePos = 0;
      int returnPos;
      int chRead = read();

      if (chRead > 0) {
        switch (chRead) {
          case '\n':
            returnPos = readlinePos;
            readlinePos = 0;  // Reset position index ready for next time.
            return returnPos;
          case '\r':
            // Ignore CR, so return -1.
            return -1;
          default:
            if (readlinePos < bufferSizeMax) {
              buffer[readlinePos++] = chRead;
              buffer[readlinePos] = 0x00; // Add NULL end of string.
            }
            // Not end of line, so return -1.
            return -1;
        }
      }

      // Not end of line, so return -1.
      return -1;
    }

    void readP1Message() {
      static bool telegramEnded = true;
      static ParsedMessage parsed = ParsedMessage();
      static uint16_t crc = 0x0000;
      static int linesRead = 0;
      static int lineLength = -1;
      static int charsCounter = 0;

      if (available()) {
        if (telegramEnded) {
          parsed = ParsedMessage();
          telegramEnded = false;
          crc = 0x0000;
          linesRead = 0;
          lineLength = -1;

          ESP_LOGD("exe", " > Receiving Telegram ...");

          // Turn off RTS when starting to receive data!
          turnOffRts();
        }

        charsCounter = 0;
        while (lineLength < 0 && charsCounter < READ_CHARS_MAX) {
          lineLength = readBufferedLineNonBlocking(readBuffer, BUF_SIZE);
          charsCounter++;
        }

        if (lineLength >= 0) {
          ESP_LOGD("data", " > Read line: \"%s\"  length: %d", readBuffer, lineLength);
          linesRead++;

          if (lineLength > 0) {
            // If we've reached the CRC checksum, calculate and read the final CRC and compare.
            if (readBuffer[0] == '!') {
              crc = crc16_update(crc, readBuffer[0]); // Include the ! in checksum.

              int crcFromMsg = (int) strtol(&readBuffer[1], NULL, 16);
              parsed.crcOk = (crc == crcFromMsg);

              ESP_LOGI("crc", " > Read Telegram %d lines. CRC: %04X = %04X : %s.",
                linesRead, crc, crcFromMsg, parsed.crcOk ? "PASS": "FAIL");

              // If the CRC pass, publish the sensor values.
              if (parsed.crcOk) {
                publishSensors(&parsed);
                statusLedTurnOn();
              } else {
                statusLedTurnOff();
              }

              telegramEnded = true;
            } else {
              // Pass the row through the CRC calculation.
              for (int i = 0; i < lineLength; i++) {
                crc = crc16_update(crc, readBuffer[i]);
              }

              // Check if this is a row containing information.
              if (strchr(readBuffer, '(') != NULL) {
                char* dataId = strtok(readBuffer, DELIMITERS);
                char* obisCode = strtok(NULL, DELIMITERS);

                // ...and this row is a data row, then parse the row.
                if (strncmp(DATA_ID, dataId, strlen(DATA_ID)) == 0) {
                  char* value = strtok(NULL, DELIMITERS);
                  char* unit = strtok(NULL, DELIMITERS);
                  parseRow(&parsed, obisCode, value);

                  statusLedToggle();
                }
              }
            }
          }

          // Remember to include carrige return and newline as it is required for the CRC calculation.
          crc = crc16_update(crc, '\r');
          crc = crc16_update(crc, '\n');

          // Reset buffer
          readBuffer[0] = 0x00;
          lineLength = -1;
        }

        if (!telegramEnded && !available()) {
          // Wait for more data...
          delay(5);
        }
      }
    }
};
