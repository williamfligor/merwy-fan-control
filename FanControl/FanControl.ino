#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>

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
static const RCSwitch::Protocol proto = { 386, { 35, 1 }, { 1, 2 }, { 2, 1 }, true };

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

void sendCmds(int fanNum, bool toggleLight, bool setFan, int fanSpeed)
{
    if (toggleLight) {
        sendState(fanNum, true, false, 0);
    }

    if (setFan) {
        sendState(fanNum, false, true, fanSpeed);
    }
}

void setup() {
    Serial.begin(115200);

    ELECHOUSE_cc1101.setGDO(GDO0_TX, GDO2_RX); 
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(FREQUENCY);

    ELECHOUSE_cc1101.SetRx();
    mySwitch.enableReceive(GDO0_TX);

    Serial.println("Setup");

    sendState(0, false, true, 3);
    sendState(4, true, true, 3);
    sendState(8, true, true, 3);
}

void loop() {
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
