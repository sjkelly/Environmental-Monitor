#include "config_user.h"


#include <SPI.h>
#include <Wire.h>

#include "Adafruit_CCS811.h"

Adafruit_CCS811 ccs;

// set up the adafruit IO feeds
AdafruitIO_Feed *temperature = io.feed("temperature");
AdafruitIO_Feed *tvoc1 = io.feed("tvoc1");
AdafruitIO_Feed *eco2 = io.feed("eco2");

float eco2_read;
float tvoc_read;
float temp_read;
float eco2_sum;
float tvoc_sum;
float temp_sum;

int num_reads;

unsigned long last_mqtt_send;

void setup() {
  Serial.begin(115200);

  if(!ccs.begin()){
    Serial.println("Failed to start sensor! Please check your wiring.");
    while(1);
  }

  delay(500);

  //calibrate temperature sensor
  while(!ccs.available());
  float temp = ccs.calculateTemperature();
  ccs.setTempOffset(temp - 25.0);

  // Adafruit IO connect
  io.connect();

  // Start Tracking time
  last_mqtt_send = millis();
  num_reads = 0;
}

void loop() {


  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  if(ccs.available()){
    temp_read = ccs.calculateTemperature();
    if(!ccs.readData()){
      Serial.print("eCO2: ");
      eco2_read = ccs.geteCO2();
      Serial.print(eco2_read);
      eco2_sum += eco2_read;

      Serial.print(" ppm, TVOC Avg: ");
      tvoc_read = ccs.getTVOC();
      Serial.print(tvoc_read);
      tvoc_sum += tvoc_read;

      Serial.print(" ppb, Temp Avg:");
      Serial.println(temp_read);
      temp_sum += temp_read;

      num_reads += 1;
    }
    else{
      Serial.println("ERROR!");
      //while(1);
    }
  }

  // wait 10 seconds
  if(millis() - last_mqtt_send > 10000) {
    float eco2_avg = eco2_sum/num_reads;
    float tvoc_avg = tvoc_sum/num_reads;
    float temp_avg = temp_sum/num_reads;

    eco2->save(eco2_avg);
    tvoc1->save(tvoc_avg);
    temperature->save(temp_avg);

    last_mqtt_send = millis();

    Serial.println("Sent MQTT Msg:");
    Serial.print("Number of readings: ");
    Serial.println(num_reads);
    Serial.print("eCO2 Avg: ");
    Serial.print(eco2_avg);

    Serial.print(" ppm, TVOC: ");
    Serial.print(tvoc_avg);

    Serial.print(" ppb, Temp:");
    Serial.println(temp_avg);

    num_reads = 0;
    eco2_sum = 0;
    temp_sum = 0;
    tvoc_sum = 0;
  }

}
