// beacon for the ae3535 communications practical exercise
 
// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption
// Passes through any wireless received messages to the serial port & responds to ACKs
// It also looks for an onboard FLASH chip, if present
// Library and code by Felix Rusu - felix@lowpowerlab.com
// Get the RFM69 and SPIFlash library at: https://github.com/LowPowerLab/
 
#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <EEPROM.h>     //Math library from arduino
#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash
 
 
#define NODEID        1    //unique for each node on same network
#define NETWORKID     10  //the same on all nodes that talk to each other
#define GATEWAYID     2    // Must be 1 or 2??
#define FREQUENCY     RF69_868MHZ
#define ENCRYPTKEY    "ae3535-practicaz" //exactly the same 16 characters/bytes on all nodes!
#define SERIAL_BAUD   115200
#define MASTERBEAT    1000
 
#define THRESHOLD_YAW 0.03
#define THRESHOLD_ROLL 0.05
 
#ifdef __AVR_ATmega1284P__
#define LED           15 // Moteino MEGAs have LEDs on D15
#define FLASH_SS      23 // and FLASH SS on D23
#else
#define LED           4 // Moteinos have LEDs on D9
#define FLASH_SS      8 // and FLASH SS on D8
#endif
 
int led = 9; // Pin 9 has an LED connected in the moteino board
int cc = 0;
 
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

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network
 
void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.print("This is the beacon ");
  delay(10);
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "listening at %d MHz...", FREQUENCY == RF69_433MHZ ? 433 : FREQUENCY == RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  if (flash.initialize()) {
    Serial.print("SPI Flash Init OK. Unique MAC = [");
    flash.readUniqueId();
    for (byte i = 0; i < 8; i++)
    {
      Serial.print(flash.UNIQUEID[i], HEX);
      if (i != 8) Serial.print(':');
    }
    Serial.println(']');
    }
  }
  //else
    //Serial.println("SPI Flash Init FAIL! (is chip present?)");

 
byte ackCount = 0;
uint32_t packetCount = 0;
int echoon = 1;
 
void loop() {
  if (radio.receiveDone()) {
    if (echoon) {
      Serial.print("#[");
      Serial.print(++packetCount);
      Serial.print(']');
      Serial.print('[');
      Serial.print(radio.SENDERID, DEC);
      Serial.print("] ");
      if (promiscuousMode) {
        Serial.print("to [");
        Serial.print(radio.TARGETID, DEC);
        Serial.print("] ");
      }
      Serial.print("Values received: \n");
 
      // Printing payload data.
      Payload* pay = (Payload*)radio.DATA;
      
      Serial.print("<NodeID "); Serial.print(pay->nodeId); Serial.print(" >\n");
      Serial.print("<Voltage 1 "); Serial.print(pay->v1); Serial.print(" >\n");
      Serial.print("<Voltage 2 "); Serial.print(pay->v2); Serial.print(" >\n");
      Serial.print("<Voltage 3 "); Serial.print(pay->v3); Serial.print(" >\n");
      
      int v1 = pay->v1;
      int v1Min = pay->v1Min;
      int v1Max = pay->v1Max;
      int v2 = pay->v2;
      int v2Min = pay->v2Min;
      int v2Max = pay->v2Max;
      int v3 = pay->v3;
      int v3Min = pay->v3Min;
      int v3Max = pay->v3Max;

  // apply the calibration to the sensor reading
  v1 = map(v1, v1Min, v1Max, 100, 255);
  v2 = map(v2, v2Min, v2Max, 100, 255);
  v3 = map(v3, v3Min, v3Max, 100, 255);

  // in case the sensor value is outside the range seen during calibration
  v1 = constrain(v1, 100, 255);
  v2 = constrain(v2, 100, 255);
  v3 = constrain(v3, 100, 255);
  
  byte brightLeft; 
  byte brightRight;
  byte brightUp;
  byte brightDown;
      
    // fade the LEDs using the calibrated value:
    if (v1 > v3 && v2 >= v1 && v2 >= v3) {
      brightLeft = v1;
      brightRight = 0.0;
      brightUp = v2;
      brightDown = 0.0;
  }
  
    if (v3 > v1 && v2 > v1 && v2 >= v3) {
      brightLeft = 0.0;
      brightRight = v3;
      brightUp = v2;
      brightDown = 0.0;
    }

    if (v3 > v1 && v2 > v1 && v2 < v3) {
      brightLeft = 0.0;
      brightRight = v3;
      brightUp = 0.0;
      brightDown = 0.0;
    }

    if (v3 > v1 && v2 <= v1 && v2 < v3) {
      brightLeft = 0.0;
      brightRight = v3;
      brightUp = 0.0;
      brightDown = v2;
    }

    if (v1 > v3 && v2 < v1 && v2 <= v3) {
      brightLeft = v1;
      brightRight = 0.0;
      brightUp = 0.0;
      brightDown = v2;
  }

    if (v1 > v3 && v2 < v1 && v2 > v3) {
      brightLeft = v1;
      brightRight = 0.0;
      brightUp = 0.0;
      brightDown = 0.0;
  }

    if (abs(v1-v3) <= 0.05 && v2 <= v1 && v2 <= v3) {
      brightLeft = 0.0;
      brightRight = 0.0;
      brightUp = v2;
      brightDown = 0.0;
  }

    if (abs(v1-v3) <= 0.05 && v2 >= v1 && v2 >= v3) {
      brightLeft = 0.0;
      brightRight = v3;
      brightUp = 0.0;
      brightDown = 0.0;
  }

    lightPackage.brightLeft = brightLeft;
    lightPackage.brightRight = brightRight;
    lightPackage.brightUp = brightUp;
    lightPackage.brightDown = brightDown;
    
    } else {
      delay(10);
    }
 
    if (radio.ACKRequested()) {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      if (echoon) Serial.println(" - ACK sent.");
    }

 
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&lightPackage), sizeof(lightPackage), 12, 300)) {
      Serial.print("GS sent data back\n");
    } else {
      Serial.print("nothing TEST...\n");
    }
  } 
//  else {
//    Serial.print("No response...");
//    Serial.print("\n");
//  }
}
 
void Blink(byte PIN, int DELAY_MS) {
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN, LOW);
}
