/****************************************************************************************************************************
  WebSockets_Generic-Impl.h - WebSockets Library for boards
  
  Based on and modified from WebSockets libarary https://github.com/Links2004/arduinoWebSockets
  to support other boards such as  SAMD21, SAMD51, Adafruit's nRF52 boards, etc.
  
  Built by Khoi Hoang https://github.com/khoih-prog/WebSockets_Generic
  Licensed under MIT license
   
  @original file WebSockets.cpp
  @date 20.05.2015
  @author Markus Sattler

  Copyright (c) 2015 Markus Sattler. All rights reserved.
  This file is part of the WebSockets for Arduino.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  Version: 2.8.0

  Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
  2.1.3   K Hoang      15/05/2020 Initial porting to support SAMD21, SAMD51, nRF52 boards, such as AdaFruit Feather nRF52832,
                                  nRF52840 Express, BlueFruit Sense, Itsy-Bitsy nRF52840 Express, Metro nRF52840 Express, etc.
  2.2.1   K Hoang      18/05/2020 Bump up to sync with v2.2.1 of original WebSockets library
  2.2.2   K Hoang      25/05/2020 Add support to Teensy, SAM DUE and STM32. Enable WebSocket Server for new supported boards.
  2.2.3   K Hoang      02/08/2020 Add support to W5x00's Ethernet2, Ethernet3, EthernetLarge Libraries. 
                                  Add support to STM32F/L/H/G/WB/MP1 and Seeeduino SAMD21/SAMD51 boards.
  2.3.1   K Hoang      07/10/2020 Sync with v2.3.1 of original WebSockets library. Add ENC28J60 EthernetENC library support
  2.3.2   K Hoang      12/11/2020 Add RTL8720DN Seeed_Arduino_rpcWiFi library support
  2.3.3   K Hoang      28/11/2020 Fix compile error for WIO_TERMINAL and boards using libraries with lib64.
  2.3.4   K Hoang      12/12/2020 Add SSL support to SAMD21 Nano-33-IoT using WiFiNINA. Upgrade WS and WSS examples.
  2.4.0   K Hoang      06/02/2021 Add support to Teensy 4.1 NativeEthernet and STM32 built-in LAN8742A. 
                                  Sync with v2.3.4 of original WebSockets library
  2.4.1   K Hoang      19/03/2021 Sync with v2.3.5 of original WebSockets library to adapt to ESP32 SSL changes 
  2.5.0   K Hoang      22/05/2021 Add support to WiFi101
  2.5.1   K Hoang      22/05/2021 Default to EIO4 for Socket.IO. Permit increase reconnectInterval in Socket.IO
  2.6.0   K Hoang      23/05/2021 Fix breaking problem with SocketIO. Add setExtraHeaders to SocketIO
  2.7.0   K Hoang      24/05/2021 Add support to RP2040-based boards using Arduino-pico and Arduino mbed_rp2040 core
  2.8.0   K Hoang      08/07/2021 Add support to WT32_ETH01 (ESP32 + LAN8720) boards
 *****************************************************************************************************************************/

#pragma once

#ifndef WEBSOCKETS_GENERIC_IMPL_H_
#define WEBSOCKETS_GENERIC_IMPL_H_

#ifdef ESP8266
  #include <core_esp8266_features.h>
#endif

#if defined(ESP32) || defined(WIO_TERMINAL)
  #ifndef CORE_HAS_LIBB64
    #define CORE_HAS_LIBB64
  #endif
#endif

extern "C"
{
  // Kludge to fix compile error for boards using libraries with lib64. From v2.3.3.
  // To find better way, such as reused #ifdef ABC_H or #define LIBRARY_USING_LIB64 or #define CORE_HAS_LIBB64 in those libraries
  #if defined(CORE_HAS_LIBB64) || defined(base64_encode_expected_len) || defined(base64_decode_expected_len)
    #warning CORE_HAS_LIBB64
    #include <libb64/cencode.h>
  #else
    #warning No CORE_HAS_LIBB64
    #include "libb64/cencode_inc.h"
  #endif
}

#ifdef ESP8266
  #include <Hash.h>
#elif defined(ESP32)

  #include <esp_system.h>

  #if ESP_IDF_VERSION_MAJOR >= 4
    #include <esp32/sha.h>
  #else
    #include <hwcrypto/sha.h>
  #endif

#else

extern "C" 
{
  #include "libsha1/libsha1.h"
}

#endif

/**

   @param client WSclient_t *  ptr to the client struct
   @param code uint16_t see RFC
   @param reason ptr to the disconnect reason message
   @param reasonLen length of the disconnect reason message
*/
void WebSockets::clientDisconnect(WSclient_t * client, uint16_t code, char * reason, size_t reasonLen)
{
  WSK_LOGDEBUG2(client->num, "[handleWebsocket] clientDisconnect code:", code);
  
  if (client->status == WSC_CONNECTED && code)
  {
    if (reason)
    {
      sendFrame(client, WSop_close, (uint8_t *) reason, reasonLen);
    }
    else
    {
      uint8_t buffer[2];
      buffer[0] = ((code >> 8) & 0xFF);
      buffer[1] = (code & 0xFF);
      sendFrame(client, WSop_close, &buffer[0], 2);
    }
  }
  
  clientDisconnect(client);
}

/**

   @param buf uint8_t *         ptr to the buffer for writing
   @param opcode WSopcode_t
   @param length size_t         length of the payload
   @param mask bool             add dummy mask to the frame (needed for web browser)
   @param maskkey uint8_t[4]    key used for payload
   @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
*/
uint8_t WebSockets::createHeader(uint8_t * headerPtr, WSopcode_t opcode, size_t length, bool mask, uint8_t maskKey[4], bool fin) 
{
  uint8_t headerSize;

  // calculate header Size
  if (length < 126)
  {
    headerSize = 2;
  }
  else if (length < 0xFFFF)
  {
    headerSize = 4;
  }
  else
  {
    headerSize = 10;
  }

  if (mask)
  {
    headerSize += 4;
  }

  // create header

  // byte 0
  *headerPtr = 0x00;

  if (fin)
  {
    *headerPtr |= bit(7);    ///< set Fin
  }

  *headerPtr |= opcode;    ///< set opcode
  headerPtr++;

  // byte 1
  *headerPtr = 0x00;

  if (mask)
  {
    *headerPtr |= bit(7);    ///< set mask
  }

  if (length < 126)
  {
    *headerPtr |= length;
    headerPtr++;
  }
  else if (length < 0xFFFF)
  {
    *headerPtr |= 126;
    headerPtr++;
    *headerPtr = ((length >> 8) & 0xFF);
    headerPtr++;
    *headerPtr = (length & 0xFF);
    headerPtr++;
  }
  else
  {
    // Normally we never get here (to less memory)
    *headerPtr |= 127;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = 0x00;
    headerPtr++;
    *headerPtr = ((length >> 24) & 0xFF);
    headerPtr++;
    *headerPtr = ((length >> 16) & 0xFF);
    headerPtr++;
    *headerPtr = ((length >> 8) & 0xFF);
    headerPtr++;
    *headerPtr = (length & 0xFF);
    headerPtr++;
  }

  if (mask)
  {
    *headerPtr = maskKey[0];
    headerPtr++;
    *headerPtr = maskKey[1];
    headerPtr++;
    *headerPtr = maskKey[2];
    headerPtr++;
    *headerPtr = maskKey[3];
    headerPtr++;
  }
  
  return headerSize;
}

/**

   @param client WSclient_t *   ptr to the client struct
   @param opcode WSopcode_t
   @param length size_t         length of the payload
   @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
   @return true if ok
*/
bool WebSockets::sendFrameHeader(WSclient_t * client, WSopcode_t opcode, size_t length, bool fin)
{
  uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
  uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

  uint8_t headerSize = createHeader(&buffer[0], opcode, length, client->cIsClient, maskKey, fin);

  if (write(client, &buffer[0], headerSize) != headerSize)
  {
    return false;
  }

  return true;
}

/**

   @param client WSclient_t *   ptr to the client struct
   @param opcode WSopcode_t
   @param payload uint8_t *     ptr to the payload
   @param length size_t         length of the payload
   @param fin bool              can be used to send data in more then one frame (set fin on the last frame)
   @param headerToPayload bool  set true if the payload has reserved 14 Byte at the beginning to dynamically 
                                add the Header (payload neet to be in RAM!)
   @return true if ok
*/
bool WebSockets::sendFrame(WSclient_t * client, WSopcode_t opcode, uint8_t * payload, size_t length, bool fin, bool headerToPayload)
{
  if (client->tcp && !client->tcp->connected())
  {
    WSK_LOGDEBUG1("[sendFrame] Not Connected!? Client:", client->num);
    
    
    return false;
  }

  if (client->status != WSC_CONNECTED)
  {
    WSK_LOGDEBUG1("[sendFrame] not in WSC_CONNECTED state!? Client:", client->num);
    
    return false;
  }

  WSK_LOGDEBUG("[sendFrame] ------- send message frame -------");
   
  WSK_LOGDEBUG1("[sendFrame] Client:", client->num);
  WSK_LOGDEBUG1("fin:",                fin); 
  WSK_LOGDEBUG1("opCode:",             opcode); 
  WSK_LOGDEBUG1("mask:",               client->cIsClient); 
  WSK_LOGDEBUG1("length:",             length);
  WSK_LOGDEBUG1("headerToPayload:",    headerToPayload);              

  if (opcode == WSop_text)
  {                     
    WSK_LOGDEBUG3("[sendFrame] Client: ", client->num, ", text:", (char*) (payload + (headerToPayload ? 14 : 0)));
  }

  uint8_t maskKey[4]                         = { 0x00, 0x00, 0x00, 0x00 };
  uint8_t buffer[WEBSOCKETS_MAX_HEADER_SIZE] = { 0 };

  uint8_t headerSize;
  uint8_t * headerPtr;
  uint8_t * payloadPtr = payload;
  bool useInternBuffer = false;
  bool ret             = true;

  // calculate header Size
  if (length < 126)
  {
    headerSize = 2;
  }
  else if (length < 0xFFFF)
  {
    headerSize = 4;
  }
  else
  {
    headerSize = 10;
  }

  if (client->cIsClient)
  {
    headerSize += 4;
  }

#ifdef WEBSOCKETS_USE_BIG_MEM
  // only for ESP since AVR has less HEAP
  // try to send data in one TCP package (only if some free Heap is there)
  if (!headerToPayload && ((length > 0) && (length < 1400)) && (GET_FREE_HEAP > 6000))
  {
    WSK_LOGDEBUG1("[sendFrame] pack to one TCP package... Client:", client->num);
    
    uint8_t * dataPtr = (uint8_t *)malloc(length + WEBSOCKETS_MAX_HEADER_SIZE);

    if (dataPtr)
    {
      memcpy((dataPtr + WEBSOCKETS_MAX_HEADER_SIZE), payload, length);
      headerToPayload = true;
      useInternBuffer = true;
      payloadPtr      = dataPtr;
    }
  }
#endif

  // set Header Pointer
  if (headerToPayload)
  {
    // calculate offset in payload
    headerPtr = (payloadPtr + (WEBSOCKETS_MAX_HEADER_SIZE - headerSize));
  }
  else
  {
    headerPtr = &buffer[0];
  }

  if (client->cIsClient && useInternBuffer)
  {
    // if we use a Intern Buffer we can modify the data
    // by this fact its possible the do the masking
    for (uint8_t x = 0; x < sizeof(maskKey); x++)
    {
      maskKey[x] = random(0xFF);
    }
  }

  createHeader(headerPtr, opcode, length, client->cIsClient, maskKey, fin);

  if (client->cIsClient && useInternBuffer)
  {
    uint8_t * dataMaskPtr;

    if (headerToPayload)
    {
      dataMaskPtr = (payloadPtr + WEBSOCKETS_MAX_HEADER_SIZE);
    }
    else
    {
      dataMaskPtr = payloadPtr;
    }

    for (size_t x = 0; x < length; x++)
    {
      dataMaskPtr[x] = (dataMaskPtr[x] ^ maskKey[x % 4]);
    }
  }

#ifndef NODEBUG_WEBSOCKETS
  unsigned long start = micros();
#endif

  if (headerToPayload)
  {
    // header has be added to payload
    // payload is forced to reserved 14 Byte but we may not need all based on the length and mask settings
    // offset in payload is calculatetd 14 - headerSize
    if (write(client, &payloadPtr[(WEBSOCKETS_MAX_HEADER_SIZE - headerSize)], (length + headerSize)) != (length + headerSize))
    {
      ret = false;
    }
  }
  else
  {
    // send header
    if (write(client, &buffer[0], headerSize) != headerSize)
    {
      ret = false;
    }

    if (payloadPtr && length > 0)
    {
      // send payload
      if (write(client, &payloadPtr[0], length) != length)
      {
        ret = false;
      }
    }
  }

#ifndef NODEBUG_WEBSOCKETS
  WSK_LOGDEBUG3("[handleWebsocketWaitFor] Sending Frame Done. Client: ", client->num, ", (us):", (micros() - start)); 
#endif  

#ifdef WEBSOCKETS_USE_BIG_MEM
  if (useInternBuffer && payloadPtr)
  {
    free(payloadPtr);
  }
#endif

  return ret;
}

/**
   callen when HTTP header is done
   @param client WSclient_t *  ptr to the client struct
*/
void WebSockets::headerDone(WSclient_t * client)
{
  client->status    = WSC_CONNECTED;
  client->cWsRXsize = 0;
  
  WSK_LOGDEBUG1("[headerDone] Header Handling Done. Client:", client->num);

#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
  client->cHttpLine = "";
  handleWebsocket(client);
#endif
}

/**
   handle the WebSocket stream
   @param client WSclient_t *  ptr to the client struct
*/
void WebSockets::handleWebsocket(WSclient_t * client)
{
  if (client->cWsRXsize == 0) 
  {
    handleWebsocketCb(client);
  }
}

/**
   wait for
   @param client
   @param size
*/
bool WebSockets::handleWebsocketWaitFor(WSclient_t * client, size_t size)
{
  if (!client->tcp || !client->tcp->connected())
  {
    return false;
  }

  if (size > WEBSOCKETS_MAX_HEADER_SIZE)
  {
    WSK_LOGDEBUG3("[handleWebsocketWaitFor] Client: ", client->num, ", size too big:", size);
    
    return false;
  }

  if (client->cWsRXsize >= size)
  {
    return true;
  }

  WSK_LOGDEBUG3("[handleWebsocketWaitFor] Client: ", client->num, ", size:", size);
  WSK_LOGDEBUG1("cWsRXsize:", client->cWsRXsize);                  
                    
                    
  readCb(client, &client->cWsHeader[client->cWsRXsize], (size - client->cWsRXsize), 
      std::bind([](WebSockets * server, size_t size, WSclient_t * client, bool ok)
  {
    WSK_LOGDEBUG3("[handleWebsocketWaitFor][readCb] Client: ", client->num, ", size:", size);
    WSK_LOGDEBUG1("ok:", ok);   

    if (ok)
    {
      client->cWsRXsize = size;
      server->handleWebsocketCb(client);
    }
    else
    {
      WSK_LOGDEBUG1("[readCb] failed. Client:", client->num);
      
      client->cWsRXsize = 0;
      // timeout or error
      server->clientDisconnect(client, 1002);
    }
  },
  this, size, std::placeholders::_1, std::placeholders::_2));
  return false;
}

void WebSockets::handleWebsocketCb(WSclient_t * client)
{
  if (!client->tcp || !client->tcp->connected())
  {
    return;
  }

  uint8_t * buffer = client->cWsHeader;

  WSMessageHeader_t * header = &client->cWsHeaderDecode;
  uint8_t * payload          = NULL;

  uint8_t headerLen = 2;

  if (!handleWebsocketWaitFor(client, headerLen))
  {
    return;
  }

  // split first 2 bytes in the data
  header->fin    = ((*buffer >> 7) & 0x01);
  header->rsv1   = ((*buffer >> 6) & 0x01);
  header->rsv2   = ((*buffer >> 5) & 0x01);
  header->rsv3   = ((*buffer >> 4) & 0x01);
  header->opCode = (WSopcode_t)(*buffer & 0x0F);
  buffer++;

  header->mask       = ((*buffer >> 7) & 0x01);
  header->payloadLen = (WSopcode_t)(*buffer & 0x7F);
  buffer++;

  if (header->payloadLen == 126)
  {
    headerLen += 2;

    if (!handleWebsocketWaitFor(client, headerLen))
    {
      return;
    }

    header->payloadLen = buffer[0] << 8 | buffer[1];
    buffer += 2;
  }
  else if (header->payloadLen == 127)
  {
    headerLen += 8;

    // read 64bit integer as length
    if (!handleWebsocketWaitFor(client, headerLen))
    {
      return;
    }

    if (buffer[0] != 0 || buffer[1] != 0 || buffer[2] != 0 || buffer[3] != 0)
    {
      // really too big!
      header->payloadLen = 0xFFFFFFFF;
    }
    else
    {
      header->payloadLen = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7];
    }

    buffer += 8;
  }

  WSK_LOGDEBUG1("[handleWebsocket] ------- read massage frame ------- Client:", client->num);

  WSK_LOGDEBUG3("[handleWebsocket] Client: ", client->num, ", fin:", header->fin); 
  WSK_LOGDEBUG1("rsv1:",    header->rsv1);
  WSK_LOGDEBUG1("rsv2:",    header->rsv2);  
  WSK_LOGDEBUG1("rsv3:",    header->rsv3);  
  WSK_LOGDEBUG1("opCode:",  header->opCode);               
                    
  WSK_LOGDEBUG3("[handleWebsocket] Client: ", client->num, ", mask:", header->mask);
  WSK_LOGDEBUG1("payloadLen:", header->payloadLen);
  
  
  if (header->payloadLen > WEBSOCKETS_MAX_DATA_SIZE)
  {
    WSK_LOGDEBUG3("[handleWebsocket] Client: ", client->num, ", payload too big:", header->payloadLen); 
    
    clientDisconnect(client, 1009);
    return;
  }

  if (header->mask)
  {
    headerLen += 4;
    if (!handleWebsocketWaitFor(client, headerLen))
    {
      return;
    }

    header->maskKey = buffer;
    buffer += 4;
  }

  if (header->payloadLen > 0)
  {
    // if text data we need one more
    payload = (uint8_t *) malloc(header->payloadLen + 1);

    if (!payload)
    {
      WSK_LOGDEBUG3("[handleWebsocket] Client: ", client->num, ", No memory to handle payload", header->payloadLen);
      
      clientDisconnect(client, 1011);
      return;
    }

    readCb(client, payload, header->payloadLen, std::bind(&WebSockets::handleWebsocketPayloadCb, 
            this, std::placeholders::_1, std::placeholders::_2, payload));
  }
  else
  {
    handleWebsocketPayloadCb(client, true, NULL);
  }
}

void WebSockets::handleWebsocketPayloadCb(WSclient_t * client, bool ok, uint8_t * payload)
{
  WSMessageHeader_t * header = &client->cWsHeaderDecode;

  if (ok)
  {
    if (header->payloadLen > 0)
    {
      payload[header->payloadLen] = 0x00;

      if (header->mask)
      {
        //decode XOR
        for (size_t i = 0; i < header->payloadLen; i++)
        {
          payload[i] = (payload[i] ^ header->maskKey[i % 4]);
        }
      }
    }
   
    switch (header->opCode)
    {
      case WSop_text:
        WSK_LOGDEBUG3("[handleWebsocketPayloadCb] Client: ", client->num, ", text:", (char *) payload);
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;

      case WSop_binary:
      case WSop_continuation:
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;
        
      case WSop_ping:
        // send pong back
        WSK_LOGDEBUG3("[handleWebsocketPayloadCb] Client: ", client->num, ", ping received", payload ? (const char *)payload : "");               
                          
        sendFrame(client, WSop_pong, payload, header->payloadLen);
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;
        
      case WSop_pong:
        WSK_LOGDEBUG3("[handleWebsocketPayloadCb] Client: ", client->num, ", get pong", payload ? (const char *)payload : "");             
                          
        client->pongReceived = true;
        messageReceived(client, header->opCode, payload, header->payloadLen, header->fin);
        break;
        
      case WSop_close:
        {
          uint16_t reasonCode = 1000;

          if (header->payloadLen >= 2)
          {
            reasonCode = payload[0] << 8 | payload[1];
          }

          WSK_LOGDEBUG3("[handleWebsocketPayloadCb] Get ask for close. Client Num:", client->num, ", Code:", reasonCode);
          
          if (header->payloadLen > 2)
          {
            WSK_LOGDEBUG1("Payload =", (char *) (payload + 2));
          }

          clientDisconnect(client, 1000);
        } 
        break;
        
      default:
        WSK_LOGDEBUG3("[WS][handleWebsocket] Got unknown opcode: Client =", client->num, ", opcode =", header->opCode);
        clientDisconnect(client, 1002);
        break;
    }

    if (payload)
    {
      free(payload);
    }

    // reset input
    client->cWsRXsize = 0;
#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
    //register callback for next message
    handleWebsocketWaitFor(client, 2);
#endif

  }
  else
  {
    WSK_LOGDEBUG1("[handleWebsocket] Missing data!. Client:", client->num);
    
    free(payload);
    clientDisconnect(client, 1002);
  }
}

/**
   generate the key for Sec-WebSocket-Accept
   @param clientKey String
   @return String Accept Key
*/
String WebSockets::acceptKey(String & clientKey)
{
  uint8_t sha1HashBin[20] = { 0 };

#ifdef ESP8266
  sha1(clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", &sha1HashBin[0]);
#elif defined(ESP32)
  String data = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  esp_sha(SHA1, (unsigned char *)data.c_str(), data.length(), &sha1HashBin[0]);
#else
  clientKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  SHA1_CTX ctx;
  SHA1Init(&ctx);
  SHA1Update(&ctx, (const unsigned char *)clientKey.c_str(), clientKey.length());
  SHA1Final(&sha1HashBin[0], &ctx);
#endif

  String key = base64_encode(sha1HashBin, 20);
  key.trim();

  return key;
}


/**
   base64_encode
   @param data uint8_t
   @param length size_t
   @return base64 encoded String
*/
String WebSockets::base64_encode(uint8_t * data, size_t length)
{
  size_t size   = ((length * 1.6f) + 1);
  char * buffer = (char *) malloc(size);
  
  //WSK_LOGDEBUG3("[base64_encode] length:", length, ", size:", size);

  if (buffer)
  {
    base64_encodestate _state;
    
    base64_init_encodestate(&_state);
    
    int len = base64_encode_block((const char *)&data[0], length, &buffer[0], &_state);

    //WSK_LOGDEBUG1("[base64_encode] #1 len:", len);
    
    len = base64_encode_blockend((buffer + len), &_state);
    //WSK_LOGDEBUG1("[base64_encode] #2 len:", len);
    
    String base64 = String(buffer);
    
    //WSK_LOGDEBUG3("[base64_encode] base64:", base64, ", buffer:", buffer);
    
    free(buffer);
    return base64;
  }
  
  return String("-FAIL-");
}

/**
   read x byte from tcp or get timeout
   @param client WSclient_t
   @param out  uint8_t * data buffer
   @param n size_t byte count
   @return true if ok
*/
bool WebSockets::readCb(WSclient_t * client, uint8_t * out, size_t n, WSreadWaitCb cb)
{
#if(WEBSOCKETS_NETWORK_TYPE == NETWORK_ESP8266_ASYNC)
  if (!client->tcp || !client->tcp->connected())
  {
    return false;
  }

  client->tcp->readBytes(out, n, std::bind([](WSclient_t * client, bool ok, WSreadWaitCb cb)
  {
    if (cb)
    {
      cb(client, ok);
    }
  }, client, std::placeholders::_1, cb));

#else
  unsigned long t = millis();
  ssize_t len;

  WSK_LOGDEBUG3("[readCb] n:", n, ", t:", t);

  while (n > 0)
  {
    if (client->tcp == NULL)
    {
      WSK_LOGDEBUG("[readCb] Null tcp!");

      if (cb)
      {
        cb(client, false);
      }
      
      return false;
    }

    if (!client->tcp->connected())
    {
      WSK_LOGDEBUG("[readCb] Not connected!");

      if (cb)
      {
        cb(client, false);
      }
      
      return false;
    }

    if ((millis() - t) > WEBSOCKETS_TCP_TIMEOUT)
    {
      WSK_LOGDEBUG1("[readCb] TIMEOUT (ms):", (millis() - t));

      if (cb)
      {
        cb(client, false);
      }
      
      return false;
    }

    if (!client->tcp->available())
    {
      WEBSOCKETS_YIELD_MORE();
      continue;
    }

    len = client->tcp->read((uint8_t *)out, n);

    if (len > 0)
    {
      t    = millis();
      out += len;
      n   -= len;
    }
    
    WSK_LOGDEBUG3("[readCb] Receive Length =", len, ", left =", n);
      
    if (n > 0) 
    {
      WEBSOCKETS_YIELD();
    }
  }

  if (cb)
  {
    cb(client, true);
  }
  
  WEBSOCKETS_YIELD();
#endif

  return true;
}

/**
   write x byte to tcp or get timeout
   @param client WSclient_t
   @param out  uint8_t * data buffer
   @param n size_t byte count
   @return bytes send
*/
size_t WebSockets::write(WSclient_t * client, uint8_t * out, size_t n)
{
  if (out == NULL)
    return 0;

  if (client == NULL)
    return 0;

  unsigned long t = millis();
  size_t len      = 0;
  size_t total    = 0;

  WSK_LOGDEBUG3("[write] n:", n, ", t:", t);

  while (n > 0)
  {
    if (client->tcp == NULL)
    {
      WSK_LOGDEBUG("[write] Null tcp!");
      break;
    }

    if (!client->tcp->connected())
    {
      WSK_LOGDEBUG("[write] Not connected!");
      break;
    }

    if ((millis() - t) > WEBSOCKETS_TCP_TIMEOUT)
    {
      WSK_LOGDEBUG1("[write] TIMEOUT (ms):", (millis() - t));
      break;
    }

    len = client->tcp->write((const uint8_t *)out, n);

    if (len)
    {
      t      = millis();
      out   += len;
      n     -= len;
      total += len;

      WSK_LOGDEBUG3("[write] Write, Length :", len, ", Left :", n);
    }
    else
    {
      WSK_LOGDEBUG3("[write] Failed Write, Length :", len, ", Left :", n);
    }
       
    if (n > 0) 
    {
      WEBSOCKETS_YIELD();
    }
  }
  
  WEBSOCKETS_YIELD();
  
  return total;
}

size_t WebSockets::write(WSclient_t * client, const char * out)
{
  if (client == NULL)
    return 0;

  if (out == NULL)
    return 0;

  return write(client, (uint8_t *)out, strlen(out));
}

/**
   enable ping/pong heartbeat process
   @param client WSclient_t
   @param pingInterval uint32_t how often ping will be sent
   @param pongTimeout uint32_t millis after which pong should timout if not received
   @param disconnectTimeoutCount uint8_t how many timeouts before disconnect, 0=> do not disconnect
*/
void WebSockets::enableHeartbeat(WSclient_t * client, uint32_t pingInterval, uint32_t pongTimeout, uint8_t disconnectTimeoutCount)
{
  if (client == NULL)
    return;

  client->pingInterval           = pingInterval;
  client->pongTimeout            = pongTimeout;
  client->disconnectTimeoutCount = disconnectTimeoutCount;
  client->pongReceived           = false;
}

/**
   handle ping/pong heartbeat timeout process
   @param client WSclient_t
*/
void WebSockets::handleHBTimeout(WSclient_t * client)
{
  if (client->pingInterval)
  {
    // if heartbeat is enabled
    uint32_t pi = millis() - client->lastPing;

    if (client->pongReceived)
    {
      client->pongTimeoutCount = 0;
    }
    else
    {
      if (pi > client->pongTimeout)
      {
        // pong not received in time
        client->pongTimeoutCount++;
        client->lastPing = millis() - client->pingInterval - 500;    // force ping on the next run

        WSK_LOGDEBUG3("[HBtimeout] pong TIMEOUT! lp=", client->lastPing, ", millis=",  millis());
        WSK_LOGDEBUG3("[HBtimeout] pong TIMEOUT! pi=", pi, ", count=", client->pongTimeoutCount);             

        if (client->disconnectTimeoutCount && client->pongTimeoutCount >= client->disconnectTimeoutCount)
        {
          WSK_LOGDEBUG1("[HBtimeout] DISCONNECTING, count=", client->pongTimeoutCount); 
          
          clientDisconnect(client);
        }
      }
    }
  }
}

#endif    // WEBSOCKETS_GENERIC_IMPL_H_
