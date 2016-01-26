/**
 * taulas_tls0.ino
 * 
 * Handles Taulas protocol to read sensors, and command switches through the Serial bus interface
 * 
 * Accepts commands using the format 'prefix'<COMMAND_NAME_AND_PARAMETERS>'suffix'
 * 
 * Examples:
 * - {OVERVIEW}
 * - {SENSOR/TEMPINT0/1}
 * 
 * Then return the result of the command between prefix and suffix too
 * If sendCommand is true, send in the beginning of the result the command and ':'
 * 
 * Examples with the command {SENSOR/TEMPINT0/1}:
 * - with sendCommand to true: {SENSOR/TEMPINT0/1:23.5}
 * - without sendCommand to true: {23.5}
 * 
 * Copyright 2016 Nicolas Mora <mail@babelouest.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU GENERAL PUBLIC LICENSE
 * License as published by the Free Software Foundation;
 * version 3 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <stdlib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

#define SERIAL_BAUD 115200

// Use different names if you have different Taulas devices on the same Angharad System
#define DEVICENAME "TLS0"

/**
 * Device wiring
 * This device has
 * - a DHT22 temperature and humidity sensor for indoor on pin 6
 * - a DS18B20 temperature sensor for outdoor temperature on pin 7
 * - a PIR Motion sensor on pin 2
 * - a light sensor on analog pin 0
 */
#define DHTPIN 2               // DHT temperature and humidity sensor (for inside)
#define DHTTYPE DHT22          // the DHT sensor is a DHT22 (more accurate than the DHT11)

#define MVTPIN 3               // PIR Movement sensor
#define MVTALERTTIMEOUT 100000 // Timeout (in milliseconds) to restart alert sending after a previous sent alert

#define DALLASPIN 4            // Dallas temperature sensor (for outside)

#define LIGHTSENSORPIN 0       // Analog ping where the light sensor is plugged into

/**
 * Structures used to facilitate sensor readings
 */
typedef struct _dhtTempHum {
  int pin;
  float temperature;
  float humidity;
  uint32_t lastTime;
} dhtTempHum;

typedef struct _mvtDetect {
  int pin;
  int value;
  unsigned long lastDetect;
  bool sent;
} mvtDetect;

DHT dht(DHTPIN, DHTTYPE);

// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(DALLASPIN);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

String  commandInput    = "";      // a string to hold incoming data
boolean commandComplete = false;   // whether the string is complete
boolean commandIncoming = false;   // wether a command is being written
boolean commandReady    = true;    // wetehr the system is ready to get a new command
int     commandTimeout  = 0;       // timeout used to flush an incomplete command
boolean commandSentBack = false;   // If true, the full command is sent back in the response

#define TIMEOUT_CYCLE 200

#define LOOP_DELAY 20 // Delay between each loop

// Prefix and suffix for all commands and results
char prefix = '{';
char suffix = '}';

// Sensors global variables
dhtTempHum dhtTempHumTab[1];

mvtDetect mvtDetectTab[1];

int lightSensorTab[1];

/**
 * Returns the Dallas sensor temperature
 */
float getDallasTemp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

/**
 * Returns the DHT temperarure
 */
float getDhtTemp(int index) {
  if (millis() - dhtTempHumTab[index].lastTime > 2000) {
    updateDht(index);
  }
  return dhtTempHumTab[index].temperature;
}

/**
 * Returns the DHT humidity
 */
float getDhtHum(int index) {
  if (millis() - dhtTempHumTab[index].lastTime > 2000) {
    updateDht(index);
  }
  return dhtTempHumTab[index].humidity;
}

/**
 * Updates the DHT sensor values
 */
boolean updateDht(int index) {
  dhtTempHumTab[index].temperature = dht.readTemperature();
  dhtTempHumTab[index].humidity = dht.readHumidity();
  dhtTempHumTab[index].lastTime = millis();
  return true;
}

/**
 * Return true if a movement is detected by the PIR sensor
 */
boolean mvtDetected(int index) {
  return digitalRead(mvtDetectTab[index].pin);
}

/**
 * Return the value returned by the light sensor
 */
float getLight(int index) {
  return analogRead(lightSensorTab[index]);
}

/**
 * Send OVERVIEW result
 * if refresh is true, force refresh of the readings
 */
void overview(boolean refresh) {
  Serial.print(prefix);
  if (commandSentBack) {
    if (refresh) {
      Serial.print("REFRESH:");
    } else {
      Serial.print("OVERVIEW:");
    }
  }
  if (refresh) {
    updateDht(0);
  }
  Serial.print("NAME:");
  Serial.print(DEVICENAME);

  Serial.print(";SENSORS");
  Serial.print(",TEMPINT0:");
  Serial.print(dhtTempHumTab[0].temperature, 1);
  
  Serial.print(",HUMINT0:");
  Serial.print(dhtTempHumTab[0].humidity, 1);
  
  Serial.print(",TEMPEXT:");
  Serial.print(getDallasTemp(), 1);
  
  Serial.print(",MVT0:");
  Serial.print(mvtDetected(0), 1);
  
  Serial.print(",LUM0:");
  Serial.print(getLight(LIGHTSENSORPIN), 1);
  
  Serial.print(suffix);
}

/**
 * Build a commandInput when one is written in the serial bus by another device
 */
void serialEvent() {
  while (Serial.available() && commandReady) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    if (commandIncoming) {
      if (inChar != suffix) {
        // add it to the commandInput:
        commandInput += inChar;
      } else {
        // if the incoming character is a suffix, set a flag
        // so the main loop can do something about it:
        commandComplete = true;
        commandIncoming = false;
        commandReady = false;
      }
    } else if (inChar == prefix) {
      commandIncoming = true;
    }
  }
}

/**
 * Initialize environnement
 */
void setup(void) {
  // start serial bus communication
  Serial.begin(SERIAL_BAUD);
  // Start up the sensor library (for external temperature)
  sensors.begin();
  
  dhtTempHumTab[0].pin = DHTPIN;
  pinMode(DHTPIN, INPUT);

  mvtDetectTab[0].pin = MVTPIN;
  pinMode(MVTPIN, INPUT);
  mvtDetectTab[0].lastDetect = millis();
  
  dht.begin();
  updateDht(0);

  lightSensorTab[0] = LIGHTSENSORPIN;
  
  pinMode(DALLASPIN, INPUT);
}

/**
 * Main loop
 * Process received commands, check motion sensor and command timeout
 */
void loop(void) {
  if (commandComplete) {
    String command = commandInput.substring(0, commandInput.indexOf("/"));
    String params = commandInput.substring(commandInput.indexOf("/")+1);
    if (command.startsWith("COMMENT:")) {
      // Comment send, do nothing
    } else if (command == "COMMANDSENDBACK") {
      if (params == "1") {
        commandSentBack = true;
      } else {
        commandSentBack = false;
      }
      if (commandSentBack) {
        Serial.print(prefix);
        Serial.print(commandInput);
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("0");
        Serial.print(suffix);
      }
    } else if (command == "NAME") {
      Serial.print(prefix);
      if (commandSentBack) {
        Serial.print(commandInput);
        Serial.print(":");
      }
      Serial.print(DEVICENAME);
      Serial.print(suffix);
    } else if (command == "MARCO") {
      Serial.print(prefix);
      if (commandSentBack) {
        Serial.print(commandInput);
        Serial.print(":");
      }
      Serial.print("POLO");
      Serial.print(suffix);
    } else if (command == "OVERVIEW") {
      overview(false);
    } else if (command == "REFRESH") {
      overview(true);
    } else if (command == "SENSOR") {
      if (params.startsWith("TEMPINT")) {
        int index = params.substring(7).toInt();
        if (commandInput.endsWith("/1")) {
          dhtTempHumTab[index].temperature = getDhtTemp(index);
        }
        Serial.print(prefix);
        if (commandSentBack) {
          Serial.print(commandInput);
          Serial.print(":");
        }
        Serial.print(dhtTempHumTab[index].temperature, 1);
        Serial.print(suffix);
      } else if (params.startsWith("HUMINT")) {
        int index = params.substring(6).toInt();
        if (params.endsWith("/1")) {
          dhtTempHumTab[index].humidity = getDhtHum(index);
        }
        Serial.print(prefix);
        if (commandSentBack) {
          Serial.print(commandInput);
          Serial.print(":");
        }
        Serial.print(dhtTempHumTab[index].humidity, 1);
        Serial.print(suffix);
      } else if (params.startsWith("TEMPEXT")) {
        Serial.print(prefix);
        if (commandSentBack) {
          Serial.print(commandInput);
          Serial.print(":");
        }
        Serial.print(getDallasTemp(), 1);
        Serial.print(suffix);
      } else if (params.startsWith("MVT")) {
        int index = params.substring(3).toInt();
        Serial.print(prefix);
        if (commandSentBack) {
          Serial.print(commandInput);
          Serial.print(":");
        }
        Serial.print(mvtDetected(index));
        Serial.print(suffix);
      } else if (params.startsWith("LUM")) {
        int index = params.substring(3).toInt();
        Serial.print(prefix);
        if (commandSentBack) {
          Serial.print(commandInput);
          Serial.print(":");
        }
        Serial.print(getLight(index));
        Serial.print(suffix);
      } else {
      }
    } else {
      Serial.print(prefix);
      if (commandSentBack) {
        Serial.print(commandInput);
        Serial.print(":");
      }
      Serial.print("Syntax error, command not found: ");
      Serial.print(commandInput);
      Serial.print(suffix);
    }
    commandInput = "";
    commandComplete = false;
    commandReady = true;
    Serial.flush();
  } else if (commandIncoming) {
    commandTimeout++;
    if (commandTimeout > TIMEOUT_CYCLE) {
      // If a command isn't complete after TIMEOUT_CYCLE cycles, flush the command
      commandReady = true;
      commandIncoming = false;
      commandInput = "";
      Serial.flush();
    }
  }

  // Mouvement sensor, send an alert if no other alert has been sent for more than MVTALERTCYCLE cycles
  if (digitalRead(mvtDetectTab[0].pin)) {
    mvtDetectTab[0].lastDetect = millis();
    if (!mvtDetectTab[0].sent) {
      Serial.print(prefix);
      Serial.print("ALERT:");
      Serial.print("MVT0");
      Serial.print(suffix);
      mvtDetectTab[0].sent = true;
    }
  } else {
    if ((millis() - mvtDetectTab[0].lastDetect) > MVTALERTTIMEOUT && mvtDetectTab[0].sent) {
      mvtDetectTab[0].sent = false;
    }
  }
  delay(LOOP_DELAY);
}
