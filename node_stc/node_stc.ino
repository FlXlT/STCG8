#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>

#define NODEID      2
#define NETWORKID   10
#define GATEWAYID   1
#define FREQUENCY   RF69_868MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "ae3535-practicaz" //has to be same 16 characters/bytes on all nodes, not more not less!
#define LED         9
#define SERIAL_BAUD 115200
#define MASTERBEAT    1000  // leave it like this please (preferable increase it)
#define ACK_TIME    30  // # of ms to wait for an ack

#ifdef __AVR_ATmega1284P__
#define LED           15 // Moteino MEGAs have LEDs on D15
#define FLASH_SS      23 // and FLASH SS on D23
#else
#define LED           9 // Moteinos have LEDs on D9
#define FLASH_SS      8 // and FLASH SS on D8
#endif

int TRANSMITPERIOD = 300; //transmit a packet to gateway so often (in ms)
byte sendSize=0;
boolean requestACK = false;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
RFM69 radio;

typedef struct {
    int           nodeId; //store this nodeId
    float         v1; // Voltages
    float         v2a;
    float         v2b;
    float         v3;
} Payload;
Payload theData;

const int ledL = 3; // Assigning the LEDS to the digital ports connected to PWM
const int ledR = 5;
const int ledU = 6;
const int ledD = 9;

void setup() {
    // Setting the pins in the correct mode
    analogReference(DEFAULT); // Using reference voltage of 3.3V
    
    pinMode(A0, INPUT); //Putting the Analog inputs to the correct mode
    pinMode(A1, INPUT);
    pinMode(A2, INPUT);
    
    pinMode(ledL, OUTPUT); //Putting the digital inputs to the correct mode
    pinMode(ledR, OUTPUT);
    pinMode(ledU, OUTPUT);
    pinMode(ledD, OUTPUT);
    
    Serial.begin(SERIAL_BAUD);
    radio.initialize(FREQUENCY,NODEID,NETWORKID);
    //radio.setHighPower(); //uncomment only for RFM69HW!
    radio.encrypt(KEY);
    char buff[50];
    sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
    
    if (flash.initialize())
        Serial.println("SPI Flash Init OK!");
    else
        Serial.println("SPI Flash Init FAIL! (is chip present?)");
}

long lastPeriod = -1;
void loop() {
    //process any serial input
    // -----------------------------------Determine voltages
    float  LDR1;
    float  LDR2a;
    float  LDR2b;
    float  LDR3;
    float  Voltage1;
    float  Voltage2;
    float  Voltage3;
    
    
    LDR1 = analogRead(A0);
    delay(10);
    LDR1 = analogRead(A0);
    delay(10);
    Voltage1 = (LDR1 * 3.3) / 1024.0;
    delay(10);
    
    LDR2a = analogRead(A1);
    delay(5000);
    LDR2b = analogRead(A1);

    Voltage2a = (LDR2a * 3.3) / 1024.0;
    Voltage2b = (LDR2b * 3.3) / 1024.0;
    
    LDR3 = analogRead(A2);
    delay(10);
    LDR3 = analogRead(A3);
    delay(10);
    Voltage3 = (LDR3 * 3.3) / 1024.0;
    delay(10);
    
    //-----------------------------fill in the struct with new values
    theData.nodeId = NODEID;
    theData.v1 = Voltage1; //
    theData.v2a = Voltage2a;
    theData.v2b = Voltage2b;
    theData.v3 = Voltage3;
    
    if (Serial.available() > 0)
    {
        char input = Serial.read();
        if (input >= 48 && input <= 57) //[0,9]
        {
            TRANSMITPERIOD = 100 * (input-48);
            if (TRANSMITPERIOD == 0) TRANSMITPERIOD = 1000;
            Serial.print("\nChanging delay to ");
            Serial.print(TRANSMITPERIOD);
            Serial.println("ms\n");
        }
        
        if (input == 'r') //d=dump register values
            radio.readAllRegs();
        //if (input == 'E') //E=enable encryption
        //  radio.encrypt(KEY);
        //if (input == 'e') //e=disable encryption
        //  radio.encrypt(null);
        
        if (input == 'd') //d=dump flash area
        {
            Serial.println("Flash content:");
            int counter = 0;
            
            while(counter<=256){
                Serial.print(flash.readByte(counter++), HEX);
                Serial.print('.');
            }
            while(flash.busy());
            Serial.println();
        }
        if (input == 'e')
        {
            Serial.print("Erasing Flash chip ... ");
            flash.chipErase();
            while(flash.busy());
            Serial.println("DONE");
        }
        if (input == 'i')
        {
            Serial.print("DeviceID: ");
            word jedecid = flash.readDeviceId();
            Serial.println(jedecid, HEX);
        }
    }
    
    //check for any received packets
    if (radio.receiveDone())
    {
        Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
        for (byte i = 0; i < radio.DATALEN; i++)
            Serial.print((char)radio.DATA[i]);
        Serial.print("   [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");
        
        if (radio.ACKRequested())
        {
            radio.sendACK();
            Serial.print(" - ACK sent");
            delay(10);
        }
        
        Serial.println();
    }
    
    int currPeriod = millis()/TRANSMITPERIOD;
    if (currPeriod != lastPeriod)
    {
        
        Serial.print("Sending struct (");
        Serial.print(sizeof(theData));
        Serial.print(" bytes) ... ");
        if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData))){
            Serial.print(" ok!");
            Serial.print(Voltage1);
            Serial.print(Voltage2);
            Serial.print(Voltage3);
        }
        else Serial.print(" nothing...");
        Serial.println();
        
        byte brightnessL;
        byte brightnessR;
        byte brightnessU;
        byte brightnessD;
        
        // check if data has been sent from the computer:
        if (Serial.available()) {
            // read the most recent byte (which will be from 0 to 255):
            brightnessL = Serial.read();
            brightnessR = Serial.read();
            brightnessU = Serial.read();
            brightnessD = Serial.read();
            // set the brightness of the LED:
            analogWrite(ledL, brightnessL);
            analogWrite(ledR, brightnessR);
            analogWrite(ledU, brightnessU);
            analogWrite(ledD, brightnessD);
        }
        
        
        lastPeriod=currPeriod;
    }
    
}



