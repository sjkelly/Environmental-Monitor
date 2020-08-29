#include "config_user.h"


#include <SPI.h>
#include <Wire.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include "driver/adc.h"

// Snesor Libs
#include "Adafruit_CCS811.h"
#include "Adafruit_PM25AQI.h"
#include "Adafruit_SHT31.h"


Adafruit_CCS811 ccs;
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// set up the adafruit IO feeds
AdafruitIO_Feed *temperature = io.feed("temperature");
AdafruitIO_Feed *humidity = io.feed("humidity");
AdafruitIO_Feed *tvoc1 = io.feed("tvoc1");
AdafruitIO_Feed *eco2 = io.feed("eco2");
AdafruitIO_Feed *pm25 = io.feed("pm2-dot-5");
AdafruitIO_Feed *pm10 = io.feed("pm1-dot-0");
AdafruitIO_Feed *pm100 = io.feed("pm10");

//CCS
float eco2_read;
float tvoc_read;
float eco2_sum;
float tvoc_sum;

//PM25
float pm10_read;
float pm25_read;
float pm100_read;
float pm10_sum;
float pm25_sum;
float pm100_sum;

//SHT
float temp_read;
float humidity_read;
float temp_sum;
float humidity_sum;

int num_reads_ccs;
int num_reads_pm;
int num_reads_sht;

unsigned long last_mqtt_send;

#define SERIALOUT 1

void setup() {

  // ESP Power saving
  btStop();
  adc_power_off();
  esp_bt_controller_disable();
  setCpuFrequencyMhz(80);


  Serial1.begin(9600); //PM2.5 UART

  #if SERIALOUT
    Serial.begin(115200);

    // Init CCS
    if(!ccs.begin()){
        Serial.println("Failed to start sensor! Please check your wiring.");
        while(1);
    }

    if (! aqi.begin_UART(&Serial1)) { // connect to the sensor over hardware serial
        Serial.println("Could not find PM 2.5 sensor!");
        while (1) delay(10);
    }
    Serial.println("PM25 found!");
  #endif


  Serial.println("SHT31 test");
  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  Serial.print("Heater Enabled State: ");
  if (sht31.isHeaterEnabled())
    Serial.println("ENABLED");
  else
    Serial.println("DISABLED");

  sht31.heater(false);

  delay(500);

  //calibrate temperature sensor
  while(!ccs.available());
    ccs.setEnvironmentalData((int)sht31.readHumidity(),sht31.readTemperature());

  // Adafruit IO connect
  io.connect();

  // Start Tracking time
  last_mqtt_send = millis();
  num_reads_ccs = 0;
  num_reads_pm = 0;
}

void loop() {

  PM25_AQI_Data data;

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  // Read CCS Sensor
  if(ccs.available()){
    if(!ccs.readData()){
      eco2_read = ccs.geteCO2();
      eco2_sum += eco2_read;
      tvoc_read = ccs.getTVOC();
      tvoc_sum += tvoc_read;

      num_reads_ccs += 1;

      #if SERIALOUT
        Serial.print("eCO2: ");
        Serial.print(eco2_read);

        Serial.print(" ppm, TVOC Avg: ");
        Serial.print(tvoc_read);
      #endif
    }
  }

  // Read AQI
  if (aqi.read(&data)) {

    num_reads_pm += 1;

    pm10_read = data.pm10_standard;
    pm25_read = data.pm25_standard;
    pm100_read = data.pm100_standard;

    pm100_sum += pm100_read;
    pm10_sum += pm10_read;
    pm25_sum += pm25_read;

    #if SERIALOUT
        Serial.println("AQI reading success");

        Serial.print(F("PM 1.0: ")); Serial.print(pm10_read);
        Serial.print(F("\t\tPM 2.5: ")); Serial.print(pm25_read);
        Serial.print(F("\t\tPM 10: ")); Serial.println(pm100_read);
    #endif
  }

  // Read SHT
  float temp_read = sht31.readTemperature();
  float humidity_read = sht31.readHumidity();

  if (! isnan(temp_read) && !isnan(humidity_read)) {  // check if 'is not a number'
    Serial.print("Temp *C = "); Serial.print(temp_read);
    Serial.print("Hum. % = "); Serial.println(humidity_read);
    temp_sum += temp_read;
    humidity_sum += humidity_read;
    num_reads_sht += 1;
  }

  // wait 30 seconds
  if(millis() - last_mqtt_send > 30000) {
    float eco2_avg = eco2_sum/num_reads_ccs;
    float tvoc_avg = tvoc_sum/num_reads_ccs;

    float temp_avg = temp_sum/num_reads_sht;
    float humidity_avg = humidity_sum/num_reads_sht;

    // Compensate CCS using SHT
    ccs.setEnvironmentalData((int)humidity_avg, temp_avg);

    float pm10_avg = pm10_sum/num_reads_pm;
    float pm25_avg = pm25_sum/num_reads_pm;
    float pm100_avg = pm100_sum/num_reads_pm;

    eco2->save(eco2_avg);
    tvoc1->save(tvoc_avg);
    temperature->save(temp_avg);
    humidity->save(humidity_avg);
    pm10->save(pm10_avg);
    pm25->save(pm25_avg);
    pm100->save(pm100_avg);

    last_mqtt_send = millis();

    #if SERIALOUT
      Serial.println("Sent MQTT Msg:");
      Serial.print("Number of CCS readings: ");
      Serial.println(num_reads_ccs);
      Serial.print("Number of PM readings: ");
      Serial.println(num_reads_pm);
      Serial.print("Number of SHT readings: ");
      Serial.println(num_reads_sht);
      Serial.print("eCO2 Avg: ");
      Serial.print(eco2_avg);

      Serial.print(" ppm, TVOC: ");
      Serial.print(tvoc_avg);

      Serial.print(" ppb, Temp:");
      Serial.println(temp_avg);

      Serial.print("PM1.0: ");
      Serial.print(pm10_avg);

      Serial.print(" PM2.5:");
      Serial.print(pm25_avg);

      Serial.print("PM10: ");
      Serial.println(pm100_avg);
    #endif

    num_reads_ccs = 0;
    num_reads_pm = 0;
    num_reads_sht = 0;
    eco2_sum = 0;
    temp_sum = 0;
    humidity_sum = 0;
    tvoc_sum = 0;
    pm100_sum = 0;
    pm10_sum = 0;
    pm25_sum = 0;
  }

}
