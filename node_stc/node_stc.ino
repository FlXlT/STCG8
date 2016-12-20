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
  float         v1; // Voltages
  float         v2;
  float         v3;
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

  // apply the calibration to the sensor reading

  v1 = map(v1, v1Min, v1Max, 100, 255);
  v2 = map(v2, v2Min, v2Max, 100, 255);
  v3 = map(v3, v3Min, v3Max, 100, 255);

  // in case the sensor value is outside the range seen during calibration
  v1 = constrain(v1, 100, 255);
  v2 = constrain(v2, 100, 255);
  v3 = constrain(v3, 100, 255);
  
  // fade the LEDs using the calibrated value:
    if (v1 > v3 && v2 >= v1 && v2 >= v3) {
      analogWrite(ledL, v1);
      analogWrite(ledR, 0.0);
      analogWrite(ledU, v2);
      analogWrite(ledD, 0.0);
  }
  
    if (v3 > v1 && v2 > v1 && v2 >= v3) {
      analogWrite(ledL, 0.0);
      analogWrite(ledR, v3);
      analogWrite(ledU, v2);
      analogWrite(ledD, 0.0);
    }

    if (v3 > v1 && v2 > v1 && v2 < v3) {
      analogWrite(ledL, 0.0);
      analogWrite(ledR, v3);
      analogWrite(ledU, 0.0);
      analogWrite(ledD, 0.0);
    }

    if (v3 > v1 && v2 <= v1 && v2 < v3) {
      analogWrite(ledL, 0.0);
      analogWrite(ledR, v3);
      analogWrite(ledU, 0.0);
      analogWrite(ledD, v2);
    }

    if (v1 > v3 && v2 < v1 && v2 <= v3) {
      analogWrite(ledL, v1);
      analogWrite(ledR, 0.0);
      analogWrite(ledU, 0.0);
      analogWrite(ledD, v2);
  }

    if (v1 > v3 && v2 < v1 && v2 > v3) {
      analogWrite(ledL, v1);
      analogWrite(ledR, 0.0);
      analogWrite(ledU, 0.0);
      analogWrite(ledD, 0.0);
  }

    if (abs(v1-v3) <= 0.05 && v2 <= v1 && v2 <= v3) {
      analogWrite(ledL, 0.0);
      analogWrite(ledR, 0.0);
      analogWrite(ledU, v2);
      analogWrite(ledD, 0.0);
  }

  
    if (abs(v1-v3) <= 0.05 && v2 >= v1 && v2 >= v3) {
      analogWrite(ledL, 0.0);
      analogWrite(ledR, v3);
      analogWrite(ledU, 0.0);
      analogWrite(ledD, 0.0);
  }
  Serial.print("Brightv1 = "); Serial.print(v1); Serial.print("\n");
  Serial.print("Brightv2 = "); Serial.print(v2); Serial.print("\n");
  Serial.print("Brightv3 = "); Serial.print(v3); Serial.print("\n");

}



