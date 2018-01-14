/*********
  Project:	Temperature Sensor with WebGUI and Syslog-Interface
  Version: 	0.0.6
  Date:		11.01.2018 / tsn
  -------------------------------------------------------------------------
  Revision:	03.01.2018 / 0.0.1 - Initialization & Tests with existing Version by Rui Santos
			05.01.2018 / 0.0.2 - Add Syslog-Support
			06.01.2018 / 0.0.3 - Add LED-Signal
			07.01.2018 / 0.0.4 - Add BlinkLed & Debug
			08.01.2018 / 0.0.5 - Move #define to Settings.h
			10.01.2018 / 0.0.6 - Tests with time.h
			
*********/

// Including the ESP8266 WiFi library
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <time.h>
#include "Settings.h"

void blinkLed(int b_led, int b_delay, int b_count);
void sendSyslog(int s_rate);
void getTemperature();

// variables
String header;
int syslog_rate = 30;   				// send all <syslog_rate> in seconds a message
int iteration = 1;
int syslog_port = 514;
int rate = S_RATE(syslog_rate);
IPAddress syslog_ip(194,191,72,90);
IPAddress ip(194,191,72,178);
IPAddress gateway(194,191,72,2);
IPAddress subnet(255,255,255,0);

// Initialize
WiFiServer server(HTTP_PORT);			// Web Server on port WIFI_PORT
WiFiUDP udpClient;						// A UDP instance to let us send and receive packets over UDP
OneWire oneWire(ONE_WIRE_BUS);			// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature DS18B20(&oneWire);	// Pass our oneWire reference to Dallas Temperature. 
char temperatureCString[6];
char temperatureFString[6];
Syslog syslog(udpClient, syslog_ip, syslog_port, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);	// Create a new syslog instance with LOG_KERN facility


// only runs once
void setup() {
  Serial.begin(115200);                 // Initializing serial port for debugging purposes
  delay(10);
  pinMode(BLUE_LED, OUTPUT);            // preparing GPIOs
  digitalWrite(BLUE_LED, HIGH);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, HIGH);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(WIFI_SSID, WIFI_PW);         // Connecting to WiFi network
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  server.begin();                         // Starting the web server
  Serial.println("Web server running. Waiting for the ESP IP...");
  Serial.println(syslog_rate);
  delay(10000);
  Serial.println(WiFi.localIP());         // Printing the ESP IP address
}

void getTemperature() {
  float tempC;
  float tempF;
  if (DEBUG == 0) {
    do {
			DS18B20.requestTemperatures(); 
			tempC = DS18B20.getTempCByIndex(0);
			dtostrf(tempC, 2, 2, temperatureCString);
			tempF = DS18B20.getTempFByIndex(0);
			dtostrf(tempF, 3, 2, temperatureFString);
      blinkLed(BLUE_LED, 100, 1);
		} while (tempC == 85.0 || tempC == (-127.0));
	}
	else {
	srand(time(NULL));
    int randC = rand();
    int randF = rand();
	dtostrf(randC, 2, 2, temperatureCString);
	dtostrf(randF, 2, 2, temperatureFString);
    blinkLed(BLUE_LED, 100, 3);
	};
}

void sendSyslog(int s_rate) {
  iteration++;
  if (iteration == s_rate) {
    getTemperature();
    iteration = 1;	
	if (DEBUG == 0) {
      syslog.logf(LOG_INFO, "Celsius %s", temperatureCString);
      syslog.logf(LOG_INFO, "Fahrenheit %s", temperatureFString);
	}
	else {
	  Serial.println("Logging to Console:");
	  Serial.println(temperatureCString);
	  Serial.println(temperatureFString);
    };
  };
}

void blinkLed(int b_led, int b_delay, int b_count) {
  for( int count = 0; count <= b_count; ++count) {
    digitalWrite(b_led, LOW);
    delay(b_delay);
    digitalWrite(b_led, HIGH);
    delay(b_delay);
  };
}

void loop() {
  sendSyslog(rate);
  WiFiClient client = server.available();				// Listenning for new clients
  if (client) {
    Serial.println("New client");
    blinkLed(RED_LED, 100, 1);
    boolean blank_line = true;							// boolean to locate when the http request ends
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n' && blank_line) {
          Serial.print(header);
          if(header.indexOf("YWRtaW46MjBiaXQ2MDk5IQ==") >= 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            if(header.indexOf("GET / HTTP/1.1") >= 0) {
              Serial.println("Main Web Page");
              getTemperature();
              client.println("<!DOCTYPE HTML>");
              client.println("<html>");
              client.println("<head></head><body><h1>ESP8266 - Temperature</h1><h3>Temperature in Celsius: ");
              client.println(temperatureCString);
              client.println("*C</h3><h3>Temperature in Fahrenheit: ");
              client.println(temperatureFString);
              client.println("*F</h3></body></html>");  
            }   
            else if(header.indexOf("GET /c HTTP/1.1") >= 0){
              Serial.println("Send only Celsius");
              getTemperature();
              client.println(temperatureCString);
            }
            else if(header.indexOf("GET /f HTTP/1.1") >= 0){
              Serial.println("Send only Fahrennheit");
              getTemperature();
              client.println(temperatureFString);
            }
          } 
          else {            
           client.println("HTTP/1.1 401 Unauthorized");
           client.println("WWW-Authenticate: Basic realm=\"Secure\"");
           client.println("Content-Type: text/html");
           client.println();
           client.println("<html>Authentication failed</html>");
          }   
          header = "";
          break;
        }
      if (c == '\n') { blank_line = true; }
      else if (c != '\r') { blank_line = false; }
      }
    }
    // closing the client connection
    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
  }
}
