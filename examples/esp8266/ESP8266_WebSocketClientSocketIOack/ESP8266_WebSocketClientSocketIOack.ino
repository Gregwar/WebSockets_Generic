/****************************************************************************************************************************
  ESP8266_WebSocketClientSocketIOack.ino
  For ESP8266

  Based on and modified from WebSockets libarary https://github.com/Links2004/arduinoWebSockets
  to support other boards such as  SAMD21, SAMD51, Adafruit's nRF52 boards, etc.

  Built by Khoi Hoang https://github.com/khoih-prog/WebSockets_Generic
  Licensed under MIT license

  Originally Created on: 20.07.2019
  Original Author: Markus Sattler
*****************************************************************************************************************************/

#if !defined(ESP8266)
  #error This code is intended to run only on the ESP8266 boards ! Please check your Tools->Board setting.
#endif

#define _WEBSOCKETS_LOGLEVEL_     3

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ArduinoJson.h>

#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>

#include <Hash.h>

ESP8266WiFiMulti WiFiMulti;
SocketIOclient socketIO;

// Select the IP address according to your local network
IPAddress serverIP(192, 168, 2, 51);
uint16_t  serverPort = 3000;

//IPAddress serverIP(10, 11, 100, 100);
//uint16_t  serverPort = 8880;

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length)
{
  switch (type)
  {
    case sIOtype_DISCONNECT:
      Serial.println("[IOc] Disconnected");
      break;
    case sIOtype_CONNECT:
      Serial.print("[IOc] Connected to url: ");
      Serial.println((char*) payload);

      break;
    case sIOtype_EVENT:
      {
        char * sptr = NULL;
        int id = strtol((char *)payload, &sptr, 10);

        Serial.print("[IOc] Get event: ");
        Serial.print((char*) payload);
        Serial.print(", id: ");
        Serial.println(id);

        if (id)
        {
          payload = (uint8_t *)sptr;
        }

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error)
        {
          Serial.print(F("DeserializeJson() failed: "));
          Serial.println(error.c_str());
          return;
        }

        String eventName = doc[0];

        Serial.print("[IOc] Event name: ");
        Serial.println(eventName);

        // Message Includes a ID for a ACK (callback)
        if (id)
        {
          // creat JSON message for Socket.IO (ack)
          DynamicJsonDocument docOut(1024);
          JsonArray array = docOut.to<JsonArray>();

          // add payload (parameters) for the ack (callback function)
          JsonObject param1 = array.createNestedObject();
          param1["now"] = millis();

          // JSON to String (serializion)
          String output;
          output += id;
          serializeJson(docOut, output);

          // Send event
          socketIO.send(sIOtype_ACK, output);
        }
      }

      break;
    case sIOtype_ACK:
      Serial.print("[IOc] Get ack: ");
      Serial.println(length);

      hexdump(payload, length);
      break;
    case sIOtype_ERROR:
      Serial.print("[IOc] Get error: ");
      Serial.println(length);

      hexdump(payload, length);
      break;
    case sIOtype_BINARY_EVENT:
      Serial.print("[IOc] Get binary: ");
      Serial.println(length);

      hexdump(payload, length);
      break;
    case sIOtype_BINARY_ACK:
      Serial.print("[IOc] Get binary ack: ");
      Serial.println(length);

      hexdump(payload, length);
      break;

    default:
      break;
  }
}

void setup() 
{
  // Serial.begin(921600);
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nStart ESP8266_WebSocketClientSocketIOack on " + String(ARDUINO_BOARD));
  Serial.println(WEBSOCKETS_GENERIC_VERSION);

  //Serial.setDebugOutput(true);

  // disable AP
  if (WiFi.getMode() & WIFI_AP) 
  {
    WiFi.softAPdisconnect(true);
  }

  WiFiMulti.addAP("SSID", "passpasspass");
  
  //WiFi.disconnect();
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.println();

  // Client address
  Serial.print("WebSockets Client started @ IP address: ");
  Serial.println(WiFi.localIP());

  // server address, port and URL
  Serial.print("Connecting to WebSockets Server @ IP address: ");
  Serial.print(serverIP);
  Serial.print(", port: ");
  Serial.println(serverPort);

  // server address, port and URL
  socketIO.begin(serverIP, serverPort);

  // event handler
  socketIO.onEvent(socketIOEvent);
}

unsigned long messageTimestamp = 0;

void loop() 
{
  socketIO.loop();

  uint64_t now = millis();

  if (now - messageTimestamp > 2000) 
  {
    messageTimestamp = now;

    // creat JSON message for Socket.IO (event)
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    // add evnet name
    // Hint: socket.on('event_name', ....
    array.add("event_name");

    // add payload (parameters) for the event
    JsonObject param1 = array.createNestedObject();
    param1["now"]     = (uint32_t) now;

    // JSON to String (serializion)
    String output;
    serializeJson(doc, output);

    // Send event
    socketIO.sendEVENT(output);

    // Print JSON for debugging
    Serial.println(output);
  }
}
