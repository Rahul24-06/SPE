#include "SparkFun_SinglePairEthernet.h"
#include <Arduino_JSON.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

SinglePairEthernet adin1110;

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme;  // I2C

JSONVar lastBMEData;
unsigned long last_report;
unsigned long last_toggle;
int sample_data_num = 0;

byte deviceMAC[6] = { 0x00, 0xE0, 0x22, 0xFE, 0xDA, 0xC9 };
byte destinationMAC[6] = { 0x00, 0xE0, 0x22, 0xFE, 0xDA, 0xCA };

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("Single Pair Ethernet - Sensor data");
  /* Start up adin1110 */
  if (!adin1110.begin(deviceMAC)) {
    Serial.print("Failed to connect to ADIN1110 MACPHY. Make sure board is connected and pins are defined for target.");
    while (1)
      ;  //If we can't connect just stop here
  }

  Serial.println("Configured ADIN1110 MACPHY");

  /* Wait for link to be established */
  Serial.println("Device Configured, waiting for connection...");
  while (adin1110.getLinkStatus() != true)
    ;

  while (bme.begin() == false)  //Begin communication over I2C
  {
    char msg[] = "The BME280 sensor did not respond. Please check wiring.";
    adin1110.sendData((byte *)msg, sizeof(msg));
    delay(1000);  //Freeze
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320*C for 150 ms
}

#define diff(a, b, max_diff) ((a >= b + max_diff) || (a <= b - max_diff))

void loop() {
  unsigned long now;
  bool force_report = false;

  //Collect the BME sensor data in an object

  if (!bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }

  JSONVar BMEData;
  BMEData["humidity"] = bme.humidity;
  BMEData["pressure"] = bme.pressure / 100.0;
  BMEData["alt"] = bme.readAltitude(SEALEVELPRESSURE_HPA);
  BMEData["temp"] = bme.temperature;
  BMEData["gas"] = bme.gas_resistance / 1000.0;

  //If any sensors have significantly changed, send a new report right away
  if (diff((double)BMEData["humidity"], (double)lastBMEData["humidity"], 2) || diff((double)BMEData["pressure"], (double)lastBMEData["pressure"], 300) || diff((double)BMEData["alt"], (double)lastBMEData["alt"], 30) || diff((double)BMEData["temp"], (double)lastBMEData["temp"], 1)) {
    force_report = true;
  }

  now = millis();
  if (now - last_report >= 5000 || force_report) {
    if (adin1110.getLinkStatus()) {
      String jsonString = JSON.stringify(BMEData);
      adin1110.sendData((byte *)jsonString.c_str(), strlen(jsonString.c_str()) + 1);
      Serial.print("Sent (");
      Serial.print(strlen(jsonString.c_str()));
      Serial.print(") bytes :\t");
      Serial.println(jsonString.c_str());

      lastBMEData = BMEData;
      last_report = now;
    } else {
      Serial.println("Waiting for link to resume sending");
    }
  }

  now = millis();
  if (now - last_toggle >= 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    last_toggle = now;
  }

  delay(100);
}