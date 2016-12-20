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
  float         v1; // Voltages
  float         v2a;
  float         v2b;
  float         v3;
} Payload;
 
Payload dataPackage;
 
typedef struct {
  byte brightLeft; //Brightness levels for the LEDS
  byte brightRight;
  byte brightUp;
  byte brightDown;
} Lightload;
 
Lightload lightPackage;

//Determination of Yaw Rotation (around z-axis; a.k.a.: "left/right rotation")
byte brightnessL; //Brightness levels for the LEDS
byte brightnessR;
byte brightnessU;
byte brightnessD;

float diffLR ; //Declaring variables for voltage differences L-R and U-D
float diffLR1;
float diffUD;
float diffUD1;
int LR1;
int UD1;
 
 
 
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
      Serial.print("<Voltage 2 "); Serial.print(pay->v2a); Serial.print(" >\n");
      Serial.print("<Voltage 2b "); Serial.print(pay->v2b); Serial.print(" >\n");
      Serial.print("<Voltage 3 "); Serial.print(pay->v3); Serial.print(" >\n");
      
      float v1 = pay->v1;
      float v2a = pay->v2a;
      float v2b = pay->v2b;
      float v3 = pay->v3;
    
    diffLR  = v1 - v3;
    //diffLR1 = constrain(diffLR, -THRESHOLD_YAW, THRESHOLD_YAW); //Constraining the value of diff LR1;

      // Maps the position of diffLR1 in the range of [0.0, 0.36] to an integer from [1, 6]
     int LR1 = map(abs(diffLR), 0.03, 0.4, 1, 6);
 
     if (diffLR > 0) {
        if (LR1 == 6) {
          brightnessL = 255.0;
          brightnessR = 0.0;
        } else {
          brightnessL = 0.0;
        
        brightnessR = LR1 * 40.0;
        }
      } else if (diffLR < 0) {
        if (LR1 == -6) {
          brightnessR = 255.0;
        } else {
          brightnessR = 0.0;
        }
        brightnessL = abs(LR1) * 40.0;
      }
    //Determination of Roll Rotation (around x-axis --> Up/Down Motion)
    diffUD = v2b - v2a;
    diffUD1 = constrain(diffUD, -THRESHOLD_ROLL, THRESHOLD_ROLL);
   int UD1 = map(abs(diffUD), 0.0, 2.0, 1, 6);
 
    if (diffUD1 == diffUD) {
      brightnessU = 255.0;
      brightnessD = 255.0;
    } else {
      if (diffUD > 0) {
        if (UD1 == 6) {
          brightnessU = 255.0;
        } else {
          brightnessU = UD1 * 40.0;
        }
        brightnessD = 0.0;
      } else {
        if (UD1 == 6) {
          brightnessD = 255.0;
        } else {
          brightnessD = UD1 * 40.0;
        }
        brightnessU = 0.0;
      }
    }
 
    lightPackage.brightLeft = brightnessL;
    lightPackage.brightRight = brightnessR;
    lightPackage.brightUp = brightnessU;
    lightPackage.brightDown = brightnessD;
    
    Serial.print(v1);Serial.print("\n");
    Serial.print(v3);Serial.print("\n");
    Serial.print(brightnessL);Serial.print("\n");
    Serial.print(brightnessR);Serial.print("\n");
    Serial.print("Constrained LR1 ");   Serial.print(diffLR1);Serial.print("\n");
    Serial.print("diffLR v1-v3 ");Serial.print(diffLR);Serial.print("\n");
    Serial.print("LR1 ");Serial.print(LR1);Serial.print("\n");
    
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
}
 
void Blink(byte PIN, int DELAY_MS) {
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN, LOW);
}
