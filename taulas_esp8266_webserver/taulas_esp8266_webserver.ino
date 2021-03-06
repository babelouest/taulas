/**
 * esp8266_taulas_webserver.ino
 * 
 * Handles communication between an arduino using Taulas protocol and an Angharad webservice
 * Communicate with distant clients with HTTP REST calls on port WEBSERVER_PORT
 * Communicate with the Taulas device via the serial port
 * 
 * - When a command is sent via the webservice, the command is sent to the Taulas device, and its response is verified, then sent back to the client
 * 
 * - When the Arduino device sends an alert message, send the message to the callback url using Gareth message format
 * 
 * **Endpoints:**
 * WEBSERVER_PREFIX/alertCb?url=<URL_CALLBACK>: set the callback url to send alerts messages
 * 
 * WEBSERVER_PREFIX?command=<COMMAND>: send a command to the Arduino device and send back the response
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
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

// Serial connection speed
#define SERIAL_BAUD 9600

// Webserver TCP port
#define WEBSERVER_PORT 858

// Set your wifi parameters
const char * ssid = "*****";
const char * password = "******";
const char prefix = '<';
const char suffix = '>';

#define LEDWIFI    2 // LEDWIFI is on when the wifi is connected
#define LEDARDUINO 0 // LEDARDUINO is on when the handshake with the Arduino is done

String alertCb = "";    // Alert callback url
String deviceName = ""; // Arduino device name

int timeoutCommand = 10000; // Timeout when an incomplete command is canceled
int timeoutAlert   = 100;   // timeout when an incomplete alert is canceled

// Communication with the Arduino structures
typedef struct _commandResult {
  String resultString;
  boolean valid;
} commandResult;

typedef struct _serialResult {
  String inputString;
  boolean stringComplete;
} serialResult;

ESP8266WebServer server(WEBSERVER_PORT);

/**
 * Send an http post to the url specified with the body specified
 */
void sendPostMessage(String url, String postBody) {
  HTTPClient http;
  int counter = 0;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(postBody);

  // Retry until it works or too much try
  while (httpCode < 0 && counter < 10) {
    counter++;
    delay(100);
    httpCode = http.POST(postBody);
  }
  if (httpCode == 200) {
    Serial.print("<COMMENT: [HTTP] POST Alert OK>");
  } else {
    Serial.printf("<COMMENT: [HTTP] POST Alert failed, error: %d>", httpCode);
  }
  http.end();
}

/**
 * Send an http get to the url specified
 */
void sendGetMessage(String url) {
  HTTPClient http;
  int counter = 0;
  http.begin(url);
  int httpCode = http.GET();

  // Retry until it works or too much try
  while (httpCode < 0 && counter < 10) {
    counter++;
    delay(100);
    httpCode = http.GET();
  }
  if (httpCode == 200) {
    Serial.print("<COMMENT: [HTTP] GET Alert OK>");
  } else {
    Serial.printf("<COMMENT: [HTTP] GET Alert failed, error: %d>", httpCode);
  }
  http.end();
}

/**
 * Read a message on the serial bus
 * A valid message must start with prefix character and end with a suffix character
 * Read until a valid message is read or timeout is reached (in milliseconds)
 */
serialResult serialRead(int timeout) {
  serialResult result;
  result.inputString = "";
  result.stringComplete = false;
  int timeCount = 0;
  String len;
  
  Serial.flush();
  while (!result.stringComplete && (timeCount < timeout)) {
    if (Serial.available() > 0) {
      // get the new byte:
      char inChar = (char)Serial.read();
      
      if ((result.inputString.length() == 0 && inChar == prefix) || (result.inputString.length() > 0 && inChar != suffix)) {
        // add it to the inputString:
        result.inputString += inChar;
      } else if (inChar == suffix) {
        result.inputString += inChar;
        result.stringComplete = true;
      }
    }
    timeCount++;
    delay(1);
  }

  return result;
}

/**
 * Send a command to the Arduino
 * then Waits for the response
 * if the response is valid, return it
 */
commandResult sendCommand(String command, int timeout) {
  commandResult cResult;
  serialResult sResult;
  Serial.print(prefix);
  Serial.print(command);
  Serial.print(suffix);
  sResult = serialRead(timeout);

  if (sResult.stringComplete && sResult.inputString.startsWith(prefix + command.substring(0, command.indexOf("/")) + ":")) {
    // Command result is valid, remove command from result (backward compatibility)
    // The command check has been added because sometimes, when 2 commands are sent at the same time
    // the response you get is not necessary the one you expect
    cResult.resultString += sResult.inputString.substring(sResult.inputString.indexOf(":") + 1, sResult.inputString.length() - 1);
    cResult.valid = true;
  } else {
    cResult.valid = false;
  }
  return cResult;
}

/**
 * Webservice callback used when an external user sends a command via the HTTP REST interface
 */
void handleCommand() {
  commandResult result;
  if (server.arg("command").length() > 0) {
    result = sendCommand(server.arg("command"), timeoutCommand);

    if (result.valid) {
      server.send(200, "application/json", result.resultString);
    } else {
      server.send(500, "application/json", "{\"error\":\"Internal error\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Error, use url: /taulas?command=<YOUR_COMMAND>\"}");
  }
  
}

/**
 * Webservice callback used when an external user sends an alertCb call via the HTTP REST interface
 */
void handleAlertCb() {
  commandResult result;
  if (server.arg("url").length() > 0) {
    result = sendCommand("URLALERT/"+server.arg("url"), timeoutCommand);
    if (result.valid) {
      server.send(200, "application/json", "{\"value\":\"ok\"}");
    } else {
      server.send(500, "application/json", "{\"error\":\"Internal error\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Error, use url: /taulas/alertCb?url=<YOUR_COMMAND>\"}");
  }
}

/**
 * Webservice called for the root (/) url
 */
void handleRoot() {
  server.send(200, "application/json", "{\"command_url\":\"/taulas?command=<YOUR_COMMAND>\",\"set_alert_url\":\"/taulas/alertCb?url=<YOUR_URL_CALLBACK>\"}");
}

/**
 * Webservice called for a 404 not found
 */
void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Not Found\"}");
}

/**
 * Read the serial bus for 'timeoutAlert' milliseconds
 * if an alert is sent from the Arduino, send it ot the callback alert url using Angharad message format
 * <alertCb>/alert/@submodule_name/@source/@element/@message/
 */
void alert() {
  if (alertCb != "" && deviceName != "") {
    serialResult result;
    String alerturlParams = "", startAlert = "{\"alert\":";
    result = serialRead(timeoutAlert);
    if (result.stringComplete && result.inputString.startsWith(startAlert)) {
      String element = result.inputString.substring(startAlert.length(), result.inputString.length()-2);
      alerturlParams = "/alert/benoic/" + deviceName + "/" + element + "/Detection";
      sendGetMessage(alertCb + alerturlParams);
    }
  }
}

/**
 * ESP8266 initialization
 * Connects to the wifi network
 * handshakes with the Arduino device
 * starts the webservice
 */
void setup(void) {
  bool setup = false;
  pinMode(LEDWIFI, OUTPUT);
  pinMode(LEDARDUINO, OUTPUT);
  commandResult result;

  digitalWrite(LEDWIFI, LOW);
  digitalWrite(LEDARDUINO, LOW);
  Serial.begin(SERIAL_BAUD);
  Serial.flush();
  WiFi.begin(ssid, password);

  // Wait for connection
  // Blinks the LEDWIFI while connecting
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LEDWIFI, !digitalRead(LEDWIFI));
    delay(500);
  }

  // Connection OK, switch on LEDWIFI
  digitalWrite(LEDWIFI, HIGH);
  
  // Handhake with the Arduino
  // Blinks the LEDARDUINO while connecting
  while (!setup) {
    result = sendCommand("MARCO", timeoutCommand);
    if (result.valid && result.resultString.endsWith("POLO\"}")) {

      result = sendCommand("NAME", timeoutCommand);
      if (result.valid) {
        deviceName = result.resultString.substring(10, result.resultString.length() - 2);
        Serial.print(prefix);
        Serial.print("COMMENT:HELLO ");
        Serial.print(deviceName);
        Serial.print(suffix);
        // Switch on LEDARDUINO, then exit
        digitalWrite(LEDARDUINO, HIGH);
        setup = true;
      } else {
        Serial.print(prefix);
        Serial.print("COMMENT:NAME ERROR ");
        Serial.print(result.resultString);
        Serial.print(suffix);
      }
    }
    if (!setup) {
      digitalWrite(LEDARDUINO, !digitalRead(LEDARDUINO));
      delay(500);
    }
  }

  // Webserver configuration
  server.begin();
  
  server.on("/", handleRoot);

  server.on("/taulas/alertCb", handleAlertCb);
  server.on("/taulas/alertCb/", handleAlertCb);

  server.on("/taulas", handleCommand);
  server.on("/taulas/", handleCommand);

  server.onNotFound(handleNotFound);

}

/**
 * loop main function
 * handles clients from the webservice and listen if an alert message is sent by the Arduino
 */
void loop(void) {
  server.handleClient();

  if (alertCb != "" && deviceName != "") {
    alert();
  }
}

