#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>

#include "options.h"

// Set receive and transmit pin numbers (GDO0 and GDO2)
#define GDO0_TX D1 // use GDO0 for TX & RX
#define GDO2_RX D2 // This is here so setGDO has something to do

// Set CC1101 frequency
// 303.631 determined from FAN-9T remote tramsmissions
#define FREQUENCY 303.631

// RC-switch settings
#define RF_LENGTH 12
#define RF_PROTOCOL 11
#define RF_REPEATS  8

RCSwitch mySwitch = RCSwitch();

const char* ssid     = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

const int led = LED_BUILTIN;

void sendState(int fanNum, bool toggleLight, bool setFan, int fanSpeed) {
    ELECHOUSE_cc1101.SetTx();
    mySwitch.disableReceive();
    mySwitch.enableTransmit(GDO0_TX);
    mySwitch.setRepeatTransmit(RF_REPEATS);
    mySwitch.setProtocol(RF_PROTOCOL);

    unsigned long control = 0;
    if (toggleLight) {
        control |= 0b1;
    } else if (setFan) {
        if (fanSpeed == 0) {
            // off
            control |= 0b10;
        }
        
        if (fanSpeed == 1) {
            // low
            control |= 0b1000;
        }
        
        if (fanSpeed == 2) {
            // med
            control |= 0b10000;
        }
        
        if (fanSpeed == 3) {
            // med
            control |= 0b100000;
        }
    }
    
    unsigned long cmd = 0;
    cmd |= ((~fanNum) & 0b1111) << 7;
    cmd |= 0b1111111 & ~control;

    Serial.print("TX'ing Command: ");
    Serial.println(cmd, BIN);

    mySwitch.send(cmd, RF_LENGTH);

    ELECHOUSE_cc1101.SetRx();
    mySwitch.disableTransmit();
    pinMode(GDO0_TX, INPUT);
    mySwitch.enableReceive(GDO0_TX);
}

int getFanId() {
    if (server.arg("id") == "") {
        return -1;
    }
    
    return server.arg("id").toInt();
}

int getFanSpeed() {
    if (server.arg("speed") == "") {
        return -1;
    }
    
    return server.arg("speed").toInt();
}

void handleLight() {
    int fanId = getFanId();
    if (fanId < 0) {
        server.send(400, "text/plain", "id parameter missing");
        return;
    }

    if (fanId > 0b1111) {
        server.send(400, "text/plain", "Invalid id");
        return;
    }
    
    sendState(fanId, true, false, 0);
    
    server.send(200, "text/plain", "ok");
}

void handleFan() {
    int fanId = getFanId();
    if (fanId < 0) {
        server.send(400, "text/plain", "id parameter missing");
        return;
    }

    if (fanId > 0b1111) {
        server.send(400, "text/plain", "Invalid id");
        return;
    }

    int fanSpeed = getFanSpeed();
    if (fanSpeed < 0) {
        server.send(400, "text/plain", "speed parameter missing");
        return;
    }

    if (fanSpeed > 3) {
        server.send(400, "text/plain", "Invalid speed");
        return;
    }
    
    sendState(fanId, false, true, fanSpeed);
    
    server.send(200, "text/plain", "ok");
}

void setup(void) {
    Serial.begin(115200);

    Serial.println("Radio initializing..");
  
    ELECHOUSE_cc1101.setGDO(GDO0_TX, GDO2_RX); 
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(FREQUENCY);

    ELECHOUSE_cc1101.SetRx();
    mySwitch.enableReceive(GDO0_TX);

    Serial.println("Radio initialized..");

    Serial.println("Wifi connecting..");

    WiFi.begin(ssid, password);
    Serial.println("");
    
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("Wifi connected");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    if (MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }
    
    server.on("/fan", handleFan);
    server.on("/light", handleLight);
    
    server.begin();
    Serial.println("HTTP server started"); 
}

void loop(void) {
    server.handleClient();

    if (mySwitch.available()) {
        unsigned long value =  mySwitch.getReceivedValue();
        unsigned int prot = mySwitch.getReceivedProtocol();
        unsigned int bits = mySwitch.getReceivedBitlength();
        unsigned int rxDelay = mySwitch.getReceivedDelay();

        Serial.print("Protocol: ");
        Serial.print(prot);
        Serial.print(" - RX Delay:  ");
        Serial.print(rxDelay);
        Serial.print(" - Value:  ");
        Serial.print(value, BIN);
        Serial.print(" - Num Bits: ");
        Serial.println(bits);

        if( prot == RF_PROTOCOL && bits == RF_LENGTH ) {
            //int id = value >> 14;
            int id = ~(value >> 7) & 0b1111;
            int cmd = value & 0b0111111;

            Serial.print("ID = ");
            Serial.print(id);
            Serial.println("");

            switch(cmd) {
                case 0b011111:
                    Serial.println("Fan High");
                    break;
                case 0b101111:
                    Serial.println("Fan Medium");
                    break;
                case 0b110111:
                    Serial.println("Fan Low");
                    break;
                case 0b111101:
                    Serial.println("Fan Off");
                    break;
                case 0b111110:
                    Serial.println("Fan Light");
                    break;
                default:
                    Serial.println("Unknown cmd");
                    break;
            }
        }

        mySwitch.resetAvailable();
    }
}
