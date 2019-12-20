#include <Arduino.h>
#include "WifiConfig.h"
#include <ESP8266WiFi.h> //buildin
#include <ArduinoOTA.h> // buildin

#ifndef WIFI_CONFIG_H
#define YOUR_WIFI_SSID "YOUR_WIFI_SSID"
#define YOUR_WIFI_PASSWD "YOUR_WIFI_PASSWD"
#endif // !WIFI_CONFIG_H

#ifndef EMON_CONFIG_H
#define EMON_NODE_ID "thermometer_id"
#define EMON_DOMAIN "examplecom"
#define EMON_PATH "emoncms" 
#define EMON_APIKEY "XXXXXXXXXXXXX"
#endif // !EMON_CONFIG_H

#define ONBOARDLED 2 // Built in LED on ESP-12/ESP-07

#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//------------------------------------------
//DS18B20
#define ONE_WIRE_BUS D3 //Pin to which is attached a temperature sensor
#define ONE_WIRE_MAX_DEV 15 //The maximum number of devices

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
int numberOfDevices; //Number of temperature devices found
DeviceAddress devAddr[ONE_WIRE_MAX_DEV];  //An array device temperature sensors
float tempDev[ONE_WIRE_MAX_DEV]; //Saving the last measurement of temperature
float tempDevLast[ONE_WIRE_MAX_DEV]; //Previous temperature measurement
long lastTemp; //The last measurement
long t, t_sent;
int dt;
const int durationTemp = 2000; //The frequency of temperature measurement
const int server_upload_period = 60000; //Upload period

//------------------------------------------
//WIFI



WiFiClient Client;

//------------------------------------------
//Convert device id to String
String GetAddressToString(DeviceAddress deviceAddress) {
  String str = "";
  for (uint8_t i = 0; i < 8; i++) {
    if ( deviceAddress[i] < 16 ) str += String(0, HEX);
    str += String(deviceAddress[i], HEX);
  }
  return str;
}

//Setting the temperature sensor
void SetupDS18B20() {

  DS18B20.begin();

  Serial.print("Parasite power is: ");
  if ( DS18B20.isParasitePowerMode() ) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  numberOfDevices = DS18B20.getDeviceCount();
  Serial.print( "Device count: " );
  Serial.println( numberOfDevices );

  lastTemp = millis();
  DS18B20.requestTemperatures();

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++) {
    // Search the wire for address
    if ( DS18B20.getAddress(devAddr[i], i) ) {
      //devAddr[i] = tempDeviceAddress;
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: " + GetAddressToString(devAddr[i]));
      Serial.println();
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }

    //Get resolution of DS18b20
    Serial.print("Resolution: ");
    Serial.print(DS18B20.getResolution( devAddr[i] ));
    Serial.println();

    //Read temperature from DS18b20
    float tempC = DS18B20.getTempC( devAddr[i] );
    Serial.print("Temp C: ");
    Serial.println(tempC);
  }
}

//Loop measuring the temperature
void TempLoop(long now) {
  if ( now - lastTemp > durationTemp ) { //Take a measurement at a fixed time (durationTemp = 5000ms, 5s)
    for (int i = 0; i < numberOfDevices; i++) {
      float tempC = DS18B20.getTempC( devAddr[i] ); //Measuring temperature in Celsius
      tempDev[i] = tempC; //Save the measured value to the array
    }
    DS18B20.setWaitForConversion(false); //No waiting for measurement
    DS18B20.requestTemperatures(); //Initiate the temperature measurement
    lastTemp = millis();  //Remember the last time measurement
  }
}


//------------------------------------------

void setup() {

  delay(1000);

  Serial.begin (115200);
  Serial.flush();
  WiFi.mode (WIFI_STA);
  WiFi.begin (YOUR_WIFI_SSID, YOUR_WIFI_PASSWD);

  pinMode (ONBOARDLED, OUTPUT); // Onboard LED
  digitalWrite (ONBOARDLED, HIGH); // Switch off LED

  //Setup DS18b20 temperature sensor
  SetupDS18B20();

  ArduinoOTA.setHostname(EMON_NODE_ID); // Задаем имя сетевого порта
  //     ArduinoOTA.setPassword((const char *)"0000"); // Задаем пароль доступа для удаленной прошивки

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
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
  });
  ArduinoOTA.begin();

}


void loop () {

  ArduinoOTA.handle(); // Всегда готовы к прошивке

  dt = millis() - t;
  t = millis();
  TempLoop( t );
  float temp1 = DS18B20.getTempC( devAddr[0] );
  //  float temp2 = DS18B20.getTempC( devAddr[1] );

  Serial.println();
  Serial.print("dt = ");
  Serial.println((t - dt) / 1000);
  Serial.print("temp1: ");
  Serial.print(temp1);
  Serial.print(", ");
  Serial.println(int(round(temp1)));

  //  Serial.print("temp2: ");
  //  Serial.print(temp2);
  //  Serial.print(", ");
  //  Serial.println(int(round(temp2)));
  //  Serial.print("dif: ");
  //  Serial.println(int(round(temp2-temp1)));


  //      if( Client.connect(EMON_DOMAIN, 80) && temp1>-50 && temp2>-50 )
  if ( Client.connect(EMON_DOMAIN, 80) && temp1 > -50 && (millis() - t_sent) > server_upload_period )
  {
    t_sent = millis();
    Serial.println("connect to Server...");
    Client.print("GET /");
    Client.print(EMON_PATH);
    Client.print("/input/post.json?apikey=");
    Client.print(EMON_APIKEY);
    Client.print("&node=");
    Client.print(EMON_NODE_ID);
    Client.print("&json={temp1:");
    Client.print(temp1);
    //          Client.print(",temp2:");
    //          Client.print(temp2);
    Client.print("}");
    Client.println();
    //          http://udom.ua/emoncms/input/post.json?node=tutu&fulljson={power1:100,power2:200,power3:300}

    unsigned long old = millis();
    while ( (millis() - old) < 500 ) // 500ms timeout for 'ok' answer from server
    {
      while ( Client.available() )
      {
        Serial.write(Client.read());
      }
    }
    Client.stop();
    Serial.println("\nclosed");
  }

  delay(5000);

}

