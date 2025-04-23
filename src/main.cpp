#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif


#define BT_DISCOVER_TIME  10000

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22); // ESP32

BluetoothSerial SerialBT;

WiFiClientSecure client;

const char* HOSTNAME = "ai.hackclub.com";

static bool btScanSync = true;

void setupWifi() {
  WiFiManager wm;
  bool res;
  res = wm.autoConnect("AutoConnectAP","password");

  if(!res) {
      Serial.println("Failed to connect");
      wm.resetSettings();
      ESP.restart();
  } 
  else {
      Serial.println("connected to WiFi" + res);
  }
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

void wifi_connecting_animation(){
    u8g2.clearBuffer();
    u8g2_prepare();
    u8g2.drawStr(0, 0, "WiFi connecting");
    u8g2.sendBuffer();
    delay(100);
    u8g2.clearBuffer();
    u8g2_prepare();
    u8g2.drawStr(0, 0, "WiFi connecting.");
    u8g2.sendBuffer();
    delay(100);
    u8g2.clearBuffer();
    u8g2_prepare();
    u8g2.drawStr(0, 0, "WiFi connecting..");
    u8g2.sendBuffer();
    delay(100);
    u8g2.clearBuffer();
    u8g2_prepare();
    u8g2.drawStr(0, 0, "WiFi connecting...");
    u8g2.sendBuffer();
    delay(1000);
}

void connect_to_wifi(){
  if (!WiFi.isConnected()) {
    Serial.println("WiFi disconnected, reconnecting...");

    setupWifi();
    
    wifi_connecting_animation();

    // Check again if connection succeeded before proceeding
    if (!WiFi.isConnected()) {
      Serial.println("WiFi reconnection failed.");
       // Optionally display failure message on screen
      u8g2.clearBuffer();
      u8g2_prepare();
      u8g2.drawStr(0, 0, "WiFi Failed!");
      u8g2.sendBuffer();
       delay(5000); // Wait before potentially retrying in the next loop iteration
       return; // Exit loop iteration if WiFi is still down
    } else {
      Serial.println("WiFi Reconnected!");
       // Optionally display success message
      u8g2.clearBuffer();
      u8g2_prepare();
      u8g2.drawStr(0, 0, "WiFi OK!");
      u8g2.sendBuffer();
      delay(1000);
    }
  }

  //! Setting client to insecure mode for testing purposes. In production, consider using secure connections.
  client.setInsecure();
}

void serverConnectedLogic(){
  const char* jsonPayload = "{\"messages\": [{\"role\": \"user\", \"content\": \"Tell me a joke!\"}]}";
  int jsonPayloadLength = strlen(jsonPayload); // Calculate the length of the payload

  client.println("POST /chat/completions HTTP/1.1"); // Use POST and HTTP/1.1

  client.print("Host: "); // Use client.print for header name
  client.println(HOSTNAME); // Use client.println for header value (adds \r\n)

  client.println("Content-Type: application/json"); // Specify JSON content

  client.print("Content-Length: "); // Specify the length of the body
  client.println(jsonPayloadLength); // The calculated length

  client.println("Connection: close"); // Close connection after response

  client.println();

  client.print(jsonPayload); // Use client.print to send the body data without extra newline

  Serial.println("Request sent. Waiting for response...");

  u8g2.clearBuffer();
  u8g2_prepare();
  u8g2.drawStr(0, 0, "Request Sent");
  u8g2.sendBuffer();

  // Wait for the server to respond or timeout
  unsigned long timeout = millis();
  while (!client.available() && millis() - timeout < 5000) {
    // Todo: Add Animations or a loading indicator
    // Wait for 5 seconds for data to become available
    delay(10);
  }

  // Check if data is available
  if (!client.available()) {
      Serial.println("No response received from server or timeout.");
      client.stop(); // Clean up the connection
      // Optionally update display
      u8g2.clearBuffer();
      u8g2_prepare();
      u8g2.drawStr(0, 0, "No Response");
      u8g2.sendBuffer();
      delay(2000);
      return; // Exit this loop iteration
  }

  Serial.println("Receiving response:");
  // Read the response headers (optional, but good for debugging)
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.print("."); // Print dots while reading headers
    if (line == "\r") {
      Serial.println("\nHeaders received.");
      break; // Found the empty line after headers
    }
    // Serial.println(line);
  }

  Serial.println("Reading response body:");
  // Read the response body
  // Consider using a JSON parsing library (like ArduinoJson) here
  // for more robust handling instead of just printing characters.
  u8g2.clearBuffer();
  u8g2_prepare();
  u8g2.setCursor(0, 0); // Set cursor for display
  u8g2.print("Response:");
  int lineNum = 1; // Track line number for display

  while (client.available()) {
    char c = client.read();
    Serial.write(c); // Print character to Serial Monitor

    // TODO: Add logic here to handle longer responses if needed (scrolling, etc.)
    // Basic display logic (might overflow screen quickly)
    if (c == '\n') {
        lineNum++;
        u8g2.setCursor(0, u8g2.getAscent() * lineNum); // Move to next line on display
    } else if (u8g2.getCursorX() < u8g2.getDisplayWidth()) {
        u8g2.print(c); // Print character to display if it fits
    }

  }
  u8g2.sendBuffer(); // Update the display with the received content

  client.stop(); // Close the connection
  Serial.println("\nConnection closed.");
}

void connect_to_server(){

  Serial.println("\nStarting connection to server for POST...");

  if (!client.connect(HOSTNAME, 443)) {
    Serial.println("Connection failed!");
    // Optionally display connection failure message
    u8g2.clearBuffer();
    u8g2_prepare();
    u8g2.drawStr(0, 10, HOSTNAME);
    u8g2.sendBuffer();
    delay(2000);
  }
  else {
    Serial.println("Connected to server!");

    u8g2.clearBuffer();
    u8g2_prepare();
    u8g2.drawStr(0, 0, "Server Connected!");
    u8g2.sendBuffer();
    delay(1000);

    serverConnectedLogic();
  }
}

void setup(void) {
  Serial.begin(115200);


  SerialBT.begin("ESP32test");
  Serial.println("The device started, now you can pair it with bluetooth!"); 
  u8g2.clearBuffer();
  u8g2_prepare();
  u8g2.drawStr(0, 0, "Bluetooth is now pairable!");
  u8g2.sendBuffer();
  if (btScanSync) {
    Serial.println("Starting discover...");
    BTScanResults *pResults = SerialBT.discover(BT_DISCOVER_TIME);
    if (pResults)
      pResults->dump(&Serial);
    else
      Serial.println("Error on BT Scan, no result!");
  }
}

void loop(void) {

  // wifi setup logic
  //connect_to_wifi();

  // wifi connection logic
  // serverConnectedLogic();

  // Add a delay at the end of the loop to prevent spamming requests
  // Serial.println("Waiting before next request...");
  // delay(10000); // Wait 10 seconds before the next loop iteration
  delay(100);
}
// Todo Refactor to multiple files