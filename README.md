Taulas
======

Multiple sensors and web interface using an Arduino UNO and a ESP8266.

Part of the Angharad project for house automation. This source code files allows to get sensors values from an HTTP interface, and send an alert to another HTTP API when the motion sensor is triggered.

# Arduino device

The Arduino used is an Arduino UNO, it has 4 different sensors:
- A DHT22 temperature and humidity sensor on pin 2
- A DS18B20 temperature sensor for outdoor temperature on pin 7
- A PIR Motion sensor on pin 2
- A light sensor on analog pin 0

## Arduino communication protocol

### Taulas protocol 1.0

The Arduino device communicates with the outside world with the serial bus with a simple protocol. A command and an answer must start and end with braces `{ }`.

The commands available in this arduino devices are:

- `{COMMENT:<any string>}`: No action
- `{COMMANDSENDBACK/<bool>}`: Send back the command in the answer for the next commands
- `{NAME}`: Send back the device name defined in constant DEVICENAME
- `{MARCO}`: Pings the Arduino, the answer must be `{POLO}` or `{MARCO:POLO}` depending on your `{COMMANDSENDBACK/<bool>}` setting
- `{OVERVIEW}/{REFRESH}`: Get the complete list of sensors and their values using the following format: `{[OVERVIEW/REFRESH:]NAME:<device name>;SENSORS,TEMPINT0:<value>,HUMINT0:<value>,TEMPEXT:<value>,MVT0:<bool>,LUM0:<value>}`. Note, `{REFRESH}` forces the reading of the values even if the timeout isn't reached.
- `{SENSOR/<sensor name>}[/1]}`: Get a specific sensor value. Note, ending the command with `/1` forces the reading of the value even if the timeout isn't reached.

Also, when the motion sensor is triggered, an alert command is sent to the serial bus. The alert has the following format: `{ALERT:<sensor name>}`.

### Taulas protocol 2.0

The command format uses `<` and `>` to delimit a command and a response, and the response data is in json format.

For example, sending the command `MARCO` to the device must be of the following form:
`<MARCO>`
And the response will have the following form:
`<MARCO:{"result":"POLO"}>`

An overview command response will have the following format:
`<OVERVIEW:{"sensors":{"SE1":{"value":33,"unit":"C"},"SE2":45},"switches":{"SW1":0},"dimmers":{"DI1":25}}>`

# ESP8266 Wifi to serial device

This device is between the Arduino UNO and the Wifi network, and allows to send command and get answers to the Arduino UNO via a HTTP Web interface. It has 2 leds wired to its pins 0 and 2. The first one blinks, and then lights on when the network is connected, the second one blinks, and then lights on when the communication is established with the Arduino UNO.

Basically, this device accepts commands on the url, transmits it to the Arduino, and then sends back the answer in the response. When an alert is triggered, the ESP8266 calls a specific HTTP API. The ESP8266 sets `COMMANDSENDBACK` to true but only to check that the answer it reads corresponds to the command it asked. The command is skipped from the answer sent to the client.

## ESP8266 HTTP API

This device has 2 different endpoints:
- `taulas/alertCb?<callback_url>`: specify the callback url api to call when an alert is triggered
- `taulas?command=<COMMAND>`: send a command to the Arduino and send back its answer in the response

# Example

When the 2 devices are on and connected, a call has the following format:

- The client calls the url: `http://esp8266.ip.address:858/taulas?command=OVERVIEW`
- The ESP8266 sends the following command to the Arduino UNO: `{OVERVIEW}`
- The Aruino UNO sends back the following answer to the ESP8266: `{OVERVIEW:NAME:TLS0;SENSORS,TEMPINT0:22.4,HUMINT0:27.6,TEMPEXT:1.3,MVT0:0,LUM0:7.0}`
- The ESP8266 skips the command in the Arduino answer and sends back to the client the following response: `{NAME:TLS0;SENSORS,TEMPINT0:22.4,HUMINT0:27.6,TEMPEXT:1.3,MVT0:0,LUM0:7.0}`

Thus, the answer can easily be parsed by a web application (as in Angharad system). The code can be easily adapted to send responses in JSON format.
