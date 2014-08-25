#include <stdlib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <dht.h>

/**
 * Modify here
 */
#define TEMPINT 11
#define TEMPEXT 7
#define HEATERPIN 2

#define LIGHTSENSOR 1
#define LIGHTPIN 3

#define DEVICENAME "DEV2"
#define PINSTART 2
#define NBPINS 4

#define NBLIGHT 1
#define NBHEATER 1
#define NBTEMPINT 1

/**
 * Goto Setup
 */

dht DHT;

typedef struct _light {
  int pin;
  int sensor;
} light;

typedef struct _heater {
  int pin;
  int sensor;
  float maxTemp;
  boolean heaterOn;
  int curCycle;
} heater;

typedef struct _tempInt {
  int sensor;
  float temperature;
} tempInt;


// Setup a oneWire instance to communicate with any OneWire devices 
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(TEMPEXT);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

int relay;
int state;
int i;
int force;
int value;
int pinStatus[NBPINS];
float tempExtStatus;

light lightTab[NBLIGHT];
heater heaterTab[NBHEATER];
tempInt tempIntTab[NBTEMPINT];

int heatCycle;

// Prefix and suffix for all output string
String prefix = "{";
String suffix = "}";

void setup(void)
{
  // start serial port
  Serial.begin(9600);
  // Start up the sensor library (for external temperature)
  sensors.begin();
  tempExtStatus = getTempExt();
  
  /**
   * Modify here
   */
  lightTab[0].pin = LIGHTPIN;
  lightTab[0].sensor = LIGHTSENSOR;
  
  heaterTab[0].pin = HEATERPIN;
  heaterTab[0].sensor = TEMPINT;
  heaterTab[0].maxTemp = 0.0;
  heaterTab[0].heaterOn = false;
  heaterTab[0].curCycle = 0;
  
  tempIntTab[0].sensor = TEMPINT;
  tempIntTab[0].temperature = getTempInt(tempIntTab[0].sensor);
  
  //int chk = DHT.read11(DHT11_PIN);
  
  /**
   * End modification
   */
  
  for (i=0; i<NBPINS; i++) {
    pinMode(i+PINSTART, OUTPUT);
    pinStatus[i] = LOW;
  }
  pinMode(TEMPEXT, INPUT);
  heatCycle = 20 * 30; // 30 secs
  
}

float getTempInt(int sensorPin) {
  int chk = DHT.read11(sensorPin);
  return DHT.temperature;
}

float getTempExt() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

boolean checkIndex(int index, int maxIndex) {
  return (index >= 0 && index < maxIndex);
}

void loop(void) {
  if (stringComplete) {
    if (inputString == "NAME\n") {
      Serial.print(prefix);
      Serial.print(DEVICENAME);
      Serial.print(suffix);
    } else if (inputString == "MARCO\n") {
      Serial.print(prefix);
      Serial.print("POLO");
      Serial.print(suffix);
    } else if (inputString == "OVERVIEW\n") {
      overview();
    } else if (inputString == "REFRESH\n") {
      refresh();
    } else if (inputString.startsWith("GETLIGHT")) {
      int index = inputString.substring(9).toInt();
      if (checkIndex(index, NBLIGHT)) {
        Serial.print(prefix);
        Serial.print(isLightOn(lightTab[index].sensor));
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("Error, light index out of bound (");
        Serial.print(index);
        Serial.print(")");
        Serial.print(suffix);
      }
    } else if (inputString.startsWith("SETLIGHT")) {
      int index = inputString.substring(9,10).toInt();
      if (checkIndex(index, NBLIGHT)) {
        if (setLight(lightTab[index].pin, lightTab[index].sensor, inputString.endsWith(",1\n"))) {
            Serial.print(prefix);
            Serial.print(inputString.endsWith(",1\n"));
            Serial.print(suffix);
        } else {
            Serial.print(prefix);
            Serial.print("Error setting light");
            Serial.print(suffix);
        }
      } else {
        Serial.print(prefix);
        Serial.print("Error, light index out of bound (");
        Serial.print(index);
        Serial.print(")");
        Serial.print(suffix);
      }
    } else if (inputString.startsWith("TEMPEXT")) {
      if (inputString.endsWith(",1\n")) {
        tempExtStatus = getTempExt();
      }
      Serial.print(prefix);
      Serial.print(tempExtStatus, 1);
      Serial.print(suffix);
    } else if (inputString.startsWith("TEMPINT")) {
      int index = inputString.substring(8).toInt();
      if (checkIndex(index, NBTEMPINT)) {
        if (inputString.endsWith(",1\n")) {
          tempIntTab[index].temperature = getTempInt(tempIntTab[index].sensor);
        }
        Serial.print(prefix);
        Serial.print(tempIntTab[index].temperature, 1);
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("Error, temperature index out of bound (");
        Serial.print(index);
        Serial.print(")");
        Serial.print(suffix);
      }
    } else if (inputString.startsWith("GETSWITCH")) {
      relay = inputString.substring(9, 10).toInt();
      force = inputString.endsWith(",1\n");
      value = getPin(relay, force);
      if (value != -1) {
        Serial.print(prefix);
        Serial.print(value);
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("Syntax error, relay "+String(relay)+" not found");
        Serial.print(suffix);
      }
    } else if (inputString.startsWith("SETSWITCH")) {
      relay = inputString.substring(9,inputString.indexOf(",")).toInt();
      state = inputString.substring(inputString.indexOf(",")+1).toInt();
      if (setPin(relay, state)) {
        Serial.print(prefix);
        Serial.print(state);
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("Syntax error, state "+String(state)+" or relay "+String(relay)+ " doesn't exists");
        Serial.print(suffix);
      }
    } else if (inputString.startsWith("SETHEATER")) {
      int index = inputString.substring(8).toInt();
      char buffer[8];
      if (checkIndex(index, NBHEATER)) {
        heaterTab[index].heaterOn = inputString.substring(11,12)=="1";
        if (!heaterTab[index].heaterOn) {
          // Force heater stop
          setPin(heaterTab[index].pin, 0 );
        } else {
          inputString.substring(13).toCharArray(buffer, sizeof(buffer));
          heaterTab[index].maxTemp = atof(buffer);
          heaterTab[index].curCycle = heatCycle-1;
          checkHeater(index);
        }
        Serial.print(prefix);
        getHeater(index);
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("Error, heater index out of bound (");
        Serial.print(index);
        Serial.print(")");
        Serial.print(suffix);
      }
    } else if (inputString.startsWith("GETHEATER")) {
      int index = inputString.substring(10).toInt();
      if (checkIndex(index, NBHEATER)) {
        Serial.print(prefix);
        getHeater(index);
        Serial.print(suffix);
      } else {
        Serial.print(prefix);
        Serial.print("Error, heater index out of bound (");
        Serial.print(index);
        Serial.print(")");
        Serial.print(suffix);
      }
    } else {
      inputString.replace("\n", "");
      if (inputString != "") {
        Serial.print(prefix);
        Serial.print("Syntax error, command '"+inputString+"' not found");
        Serial.print(suffix);
      }
    }
    inputString = "";
    stringComplete = false;
    Serial.flush();
  }
  for (i = 0; i < NBHEATER; i++) {
    if (heaterTab[i].heaterOn) {
      heaterTab[i].curCycle++;
      heaterTab[i].curCycle %= heatCycle;
    } else {
      heaterTab[i].curCycle = 0;
    }
    checkHeater(i);
  }
  delay(50);
}

/*
 *
 */
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    } 
  }
}

/*
 *
 */
void overview() {
  Serial.print(prefix);
  Serial.print("NAME:");
  Serial.print(DEVICENAME);
  for (i=0; i<NBPINS; i++) {
    Serial.print(",PIN");
    Serial.print((i+PINSTART));
    Serial.print(":");
    Serial.print(pinStatus[i]);
  }
  for (i=0; i<NBLIGHT; i++) {
    Serial.print(",LIGHT");
    Serial.print(i);
    Serial.print(":");
    Serial.print(isLightOn(lightTab[i].sensor));
  }
  Serial.print(",TEMPEXT:");
  Serial.print(tempExtStatus, 1);
  for (i=0; i<NBTEMPINT; i++) {
    Serial.print(",TEMPINT");
    Serial.print(i);
    Serial.print(":");
    Serial.print(tempIntTab[i].temperature, 1);
  }
  for (i=0; i<NBHEATER; i++) {
    Serial.print(",HEATER");
    Serial.print(i);
    Serial.print(":");
    getHeater(i);
  }
  Serial.print(suffix);
}

/*
 *
 */
void refresh() {
  for (i=0; i<NBPINS; i++) {
    getPin(i+PINSTART, true);
  }
  tempExtStatus = getTempExt();
  for (i=0; i<NBTEMPINT; i++) {
    tempIntTab[i].temperature = getTempInt(tempIntTab[i].sensor);
  }
  overview();
}

/*
 *
 */
int getPin(int pin, bool hard) {
  if (pin >= PINSTART && pin <= PINSTART+NBPINS) {
    if (hard) {
      pinStatus[pin-PINSTART] = digitalRead(pin);
    }
    return pinStatus[pin-PINSTART];
  } else {
    return -1;
  }
}

/*
 *
 */
bool setPin(int pin, int state) {
  if (pin >= PINSTART && pin <= PINSTART+NBPINS && (state == LOW || state == HIGH)) {
    digitalWrite(pin, state);
    pinStatus[pin-PINSTART] = state;
    return true;
  } else {
    return false;
  }
}

void setHeater(int index, boolean on, float maxTemp) {
  heaterTab[index].heaterOn = on;
  heaterTab[index].maxTemp = maxTemp;
}

void getHeater(int index) {
  pinStatus[heaterTab[index].pin-PINSTART] = digitalRead(heaterTab[index].pin);
  Serial.print(heaterTab[index].heaterOn);
  Serial.print(";");
  Serial.print(pinStatus[heaterTab[index].pin-PINSTART]);
  Serial.print(";");
  Serial.print(heaterTab[index].maxTemp);
}

/**
 *
 */
int checkHeater(int index) {
  if (!heaterTab[index].heaterOn) {
    return 0;
  } else if (heaterTab[index].curCycle == 0) {
    float curTemp = getTempInt(heaterTab[index].sensor);
    setPin(heaterTab[index].pin, (heaterTab[index].maxTemp > (curTemp+1)) );
    return getPin(heaterTab[index].pin, false);
  } else {
    return -2;
  }
}

boolean isLightOn(int analogPin) {
  int count = 0;
  float currentSum = 0.0;
  int counter = 50;
  while (count < counter) {
    currentSum += abs(analogRead(analogPin)-510);
    count++;
    delay(1);
  }
  return ((currentSum/counter) > 1);
}

boolean setLight(int lightPin, int sensorPin, boolean state) {
  if ((isLightOn(sensorPin) && !state) || (!isLightOn(sensorPin) && state)) {
    return (setPin(lightPin, !getPin(lightPin, false)));
  }
  return true;
}
