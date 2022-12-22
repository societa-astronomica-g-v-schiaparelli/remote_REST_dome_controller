// Company: KMP Electronics Ltd, Bulgaria
// Web: https://kmpelectronics.eu/
/*
  Ethernet Modbus TCP Server LED

  This sketch creates a Modbus TCP Server with a simulated coil.
  The value of the simulated coil is set on the LED

  Circuit:
   - ProDino ESP32 Ethernet

  created 24 Aug 2022
  by Dimitar Antonov
*/

#include "KMPProDinoESP32.h"
#include "KMPCommon.h"
#include <ArduinoModbus.h>

// Enter a MAC address and IP address for your controller below.
byte _mac[] = { 0x00, 0x08, 0xDC, 0x72, 0xE7, 0x40 };
// The IP address will be dependent on your local network.
IPAddress _ip(192, 168, 1, 197);

#define SERVER_PORT 502

// Initialize the Ethernet server library.
// with the IP address and port you want to use.
EthernetServer ethServer(SERVER_PORT);

ModbusTCPServer modbusTCPServer;

/**
* @brief Setup void. It is Arduino executed first. Initialize DiNo board.
*
*
* @return void
*/
void setup()
{
	delay(500);
	Serial.begin(115200);

  Serial.println("Ethernet Modbus TCP Example");

	// Init Dino board. Set pins, start W5500.
	KMPProDinoESP32.begin(ProDino_ESP32_Ethernet);
	//KMPProDinoESP32.begin(ProDino_ESP32_Ethernet_GSM);
	//KMPProDinoESP32.begin(ProDino_ESP32_Ethernet_LoRa);
	//KMPProDinoESP32.begin(ProDino_ESP32_Ethernet_LoRa_RFM);
	KMPProDinoESP32.setStatusLed(blue);
	
	// Start the Ethernet connection and the server.
	// Using static IP address
	//Ethernet.begin(_mac, _ip);
	// Getting IP from DHCP
	if (Ethernet.begin(_mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
		// no point in carrying on, so do nothing forevermore:
		while (1);
	}

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("W5500 was not found!");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

	Serial.println("Ethernet IP:");
	Serial.print(Ethernet.localIP());
	Serial.print(":");
	Serial.println(SERVER_PORT);

  // start the server
  ethServer.begin();
  
  // start the Modbus TCP server
  if (!modbusTCPServer.begin()) {
    Serial.println("Failed to start Modbus TCP Server!");
    while (1);
  }

  // configure a single coil at address 0x00
  modbusTCPServer.configureCoils(0x00, 4);

  KMPProDinoESP32.offStatusLed();

}

/**
* @brief Loop void. Arduino executed second.
*
*
* @return void
*/
void loop()
{

    KMPProDinoESP32.processStatusLed(green, 300);

    // listen for incoming clients
  EthernetClient client = ethServer.available();
  
  if (client) {
    // a new client connected
    Serial.println("new client");

    KMPProDinoESP32.setStatusLed(green);
    // let the Modbus TCP accept the connection 
    modbusTCPServer.accept(client);

    while (client.connected()) {
      // poll for Modbus TCP requests, while client connected
      modbusTCPServer.poll();

      // update the IO
      updateIO();
    }

    Serial.println("client disconnected");
  }

}

void updateIO() {

    for (uint8_t i = 0; i < 4; i++)
    {
        if (modbusTCPServer.coilRead(i)) {
            // coil value set, turn RELAY on
            KMPProDinoESP32.setRelayState(i, true);
        }
        else {
            // coild value clear, turn RELAY off
            KMPProDinoESP32.setRelayState(i, false);
        }
    }

}
