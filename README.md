Taulas
======

Multiple sensors and web interface using an Arduino UNO and an HTTP interface (ESP8266 or linux).

Part of the Angharad project for house automation. This source code files allows to get sensors values from an HTTP interface, and send an alert to another HTTP API when the motion sensor is triggered.

# Arduino device

The Arduino used is an Arduino UNO, it has 4 different sensors:
- A DHT22 temperature and humidity sensor on pin 2
- A DS18B20 temperature sensor for outdoor temperature on pin 7
- A PIR Motion sensor on pin 2
- A light sensor on analog pin 0

## Arduino communication protocol

### Taulas protocol 1.0

The Arduino device communicates with the outside world with the serial bus with a simple protocol. A command and an answer must start and end with diples `< >`.

The commands available in this arduino devices are:

- `<COMMENT:any string>`: No action
- `<NAME>`: Send back the device name defined in constant DEVICENAME
- `<MARCO>`: Pings the Arduino, the answer must be `<MARCO:{"result":"POLO"}>`
- `<OVERVIEW>`: Get the complete list of sensors and their values using the following format: ``<OVERVIEW:{"sensors":{"SE1":{"value":33,"unit":"C"},"SE2":45},"switches":{"SW1":0},"dimmers":{"DI1":25}}>`.
- `<SENSOR/sensor_name>`: Get a specific sensor value, the answer uses the format `<SENSOR:{"value":sensor_value}>`.

Also, when the motion sensor is triggered, an alert command is sent to the serial bus. The alert has the following format: `<ALERT:sensor_name>`.

### Taulas protocol 2.0

The command format uses `<` and `>` to delimit a command and a response, and the response data is in json format.

For example, sending the command `MARCO` to the device must be of the following form:
`<MARCO>`
And the response will have the following format:
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

# Raspberry Pi to serial device

A taulas interface application has been implemented for a Raspberry Pi (or any linux based device) using a USB cable between the RPi and the Arduino Taulas device.

The source code is available in the `taulas_raspberrypi_serial` folder. You need [Ulfius](https://github.com/babelouest/ulfius) to compile. When Ulfius and its dependencies are installed, run the following commands:

```shell
$ make
$ ./taulas-rpi-serial
```

This program has default parameters that can be overwritten, the online help will provide the list of parameters:

```shell
$ ./taulas-rpi-serial -h

taulas-rpi-serial, serial interface with a taulas arduino device
Options available:
-h --help: Print this help message and exit
-p --port: TCP Port to listen to, default 8585
-u --url-prefix: url prefix for the webservice, default 'taulas'
-s --serial-pattern: pattern to the serial file of the arduino, default '/dev/ttyACM'
-b --baud: baud rate to connect to the Arduino, default 9600
-t --timeout: timeout in seconds for serial reading, default 3000
-l --log-level: log level for the application, values are NONE, ERROR, WARNING, INFO, DEBUG, default is 'DEBUG'
-m --log-mode: log mode for the application, values are console, file or syslog, multiple values must be separated with a comma, default is 'console'
-f --log-file: path to log file if log mode is file
```

# Example with Taulas 2.0 protocol

When the 2 devices are on and connected, a call has the following format:

- The client calls the url: `http://esp8266.ip.address:858/taulas?command=OVERVIEW` for an ESP8266 or `http://rpi.ip.address:8585/taulas/?command=OVERVIEW` for taulas-rpi-serial.
- The ESP8266 or the Rpi sends the following command to the Arduino UNO: `<OVERVIEW>`
- The Aruino UNO sends back the following answer: `<OVERVIEW:{"sensors":{"SE1":{"value":33,"unit":"C"},"SE2":45},"switches":{"SW1":0},"dimmers":{"DI1":25}}>`
- The ESP8266 or the RPI skips the command in the Arduino answer and sends back to the client the following response: `{"sensors":{"SE1":{"value":33,"unit":"C"},"SE2":45},"switches":{"SW1":0},"dimmers":{"DI1":25}}`

