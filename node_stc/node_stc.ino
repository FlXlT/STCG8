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
#define MASTERBEAT  2000  // leave it like this please (preferable increase it)
#define ACK_TIME    500  // # of ms to wait for an ack

#ifdef __AVR_ATmega1284P__
#define LED           15 // Moteino MEGAs have LEDs on D15
#define FLASH_SS      23 // and FLASH SS on D23
#else
#define LED           9 // Moteinos have LEDs on D9
#define FLASH_SS      8 // and FLASH SS on D8
#endif

int TRANSMITPERIOD = 500; //transmit a packet to gateway so often (in ms)
byte sendSize = 0;
boolean requestACK = false;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
RFM69 radio;

typedef struct {
  int           nodeId; //store this nodeId
  int         v1; // Voltages
  int         v1Min;
  int         v1Max;
  int         v2;
  int         v2Min;
  int         v2Max;
  int         v3;
  int         v3Min;
  int         v3Max;
} Payload;
Payload theData;

typedef struct {
  byte brightLeft; //Brightness levels for the LEDS
  byte brightRight;
  byte brightUp;
  byte brightDown;
} Lightload;

Lightload lightPackage;

//Constants that won't change
const int ledL = 6; // Assigning the LEDS to the digital ports connected to PWM
const int ledR = 9;
const int ledU = 5;
const int ledD = 3;

const int LDR1 = A1; // Assigning the sensors to the pins
const int LDR2 = A0;
const int LDR3 = A2;

//Variables
int v1 = 0;         // Sensor Value
int v1Min = 1023;   // Minimum Sensor Value
int v1Max = 0;      // Maximum Sensor Value

int v2 = 0;
int v2Min = 1023;
int v2Max = 0;

int v3 = 0;
int v3Min = 1023;
int v3Max = 0;

byte brightnessL;
byte brightnessR;
byte brightnessU;
byte brightnessD;

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
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  //radio.setHighPower(); //uncomment only for RFM69HW!
  radio.encrypt(KEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY == RF69_433MHZ ? 433 : FREQUENCY == RF69_868MHZ ? 868 : 915);
  Serial.println(buff);

  if (flash.initialize())
    Serial.println("SPI Flash Init OK!");
  else
    Serial.println("SPI Flash Init FAIL! (is chip present?)");

  // turn on LEDs to signal the start of the calibration period:
  analogWrite(ledL, 255.0);
  analogWrite(ledR, 255.0);
  analogWrite(ledU, 255.0);
  analogWrite(ledD, 255.0);

  // calibrate during the first 10 seconds
  while (millis() < 10000) {
    v1 = analogRead(LDR1);
    v2 = analogRead(LDR2);
    v3 = analogRead(LDR3);

    // record the maximum sensor value
    if (v1 > v1Max) {
      v1Max = v1;
    }

    if (v2 > v2Max) {
      v2Max = v2;
    }

    if (v3 > v3Max) {
      v3Max = v3;
    }

    // record the minimum sensor value
    if (v1 < v1Min) {
      v1Min = v1;
    }

    if (v2 < v2Min) {
      v2Min = v2;
    }

    if (v3 < v3Min) {
      v3Min = v3;
    }
  }

  // signal the end of the calibration period
  analogWrite(ledL, 0.0);
  analogWrite(ledR, 0.0);
  analogWrite(ledU, 0.0);
  analogWrite(ledD, 0.0);
}

void loop() {
  long lastPeriod = -1;

  // -----------------------------------Determine voltages
  // read the sensor:
  v1 = analogRead(LDR1);
  v2 = analogRead(LDR2);
  v3 = analogRead(LDR3);

  //-----------------------------fill in the struct with new values
  theData.nodeId = NODEID;
  theData.v1 = v1;
  theData.v1Min = v1Min;
  theData.v1Max = v1Max;
  theData.v2 = v2;
  theData.v2Min = v2Min;
  theData.v2Max = v2Max;
  theData.v3 = v3;
  theData.v3Min = v3Min;
  theData.v3Max = v3Max;

  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input >= 48 && input <= 57) //[0,9]
    {
      TRANSMITPERIOD = 100 * (input - 48);
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

      while (counter <= 256) {
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      while (flash.busy());
      Serial.println();
    }
    if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while (flash.busy());
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
    Serial.print('['); Serial.print(radio.SENDERID, DEC); Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:"); Serial.print(radio.readRSSI()); Serial.print("]");

    if (radio.ACKRequested())
    {
      radio.sendACK();
      Serial.print(" - ACK sent");
      delay(10);
    }

    Serial.println();
  }

  int currPeriod = millis() / TRANSMITPERIOD;
  if (currPeriod != lastPeriod)
  {

    Serial.print("Sending struct (");
    Serial.print(sizeof(theData));
    Serial.print(" bytes) ... ");
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData))) {
      Serial.print(" ok!");
      Serial.print(v1);
      Serial.print(v2);
      Serial.print(v3);
    }
    else Serial.print(" nothing...");
    Serial.println();

    // check if data has been sent from the computer:
    // Printing payload data.
    Lightload* light = (Lightload*)radio.DATA;
    Serial.read();
    // read the most recent byte (which will be from 0 to 255):
    brightnessL = light->brightLeft;
    brightnessR = light->brightRight;
    brightnessU = light->brightUp;
    brightnessD = light->brightDown;
    // set the brightness of the LED:
    analogWrite(ledL, brightnessL);
    analogWrite(ledR, brightnessR);
    analogWrite(ledU, brightnessU);
    analogWrite(ledD, brightnessD);
  }

}



