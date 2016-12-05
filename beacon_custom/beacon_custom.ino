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
#define GATEWAYID     1    // Must be 1 or 2??
#define FREQUENCY     RF69_868MHZ
#define ENCRYPTKEY    "ae3535-practicaz" //exactly the same 16 characters/bytes on all nodes!
#define SERIAL_BAUD   115200

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
int cc=0;

byte brightnessL; //Brightness levels for the LEDS
byte brightnessR;
byte brightnessU;
byte brightnessD;

float v2a;
float v2b;
float diffLR ; //Declaring variables for voltage differences L-R and U-D
float diffLR1;
float diffUD;
float diffUD1;
int LR1;
int UD1;


typedef struct {
    int           nodeId; //store this nodeId
    float         v1; // Voltages
    float         v2a;
    float         v2b;
    float         v3;
} Payload;

Payload dataPackage;

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.print("This is the beacon ");
    delay(10);
    radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
    radio.setHighPower(); //only for RFM69HW!
#endif
    radio.encrypt(ENCRYPTKEY);
    //radio.promiscuous(promiscuousMode);
    //radio.setFrequency(919000000);
    char buff[50];
    sprintf(buff, "listening at %d MHz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
    //if (flash.initialize())
    if (flash.initialize()) {
        Serial.print("SPI Flash Init OK. Unique MAC = [");
        flash.readUniqueId();
        for (byte i=0;i<8;i++)
        {
            Serial.print(flash.UNIQUEID[i], HEX);
            if (i!=8) Serial.print(':');
        }
        Serial.println(']');
        //alternative way to read it:
        //byte* MAC = flash.readUniqueId();
        //for (byte i=0;i<8;i++)
        //{
        //  Serial.print(MAC[i], HEX);
        //  Serial.print(' ');
        //}
    }
    else
        Serial.println("SPI Flash Init FAIL! (is chip present?)");
}

byte ackCount=0;
uint32_t packetCount = 0;
int echoon = 1;

void loop() {
    //process any serial input
    if (Serial.available() > 0)
    {
        char input = Serial.read();
        if (input == 'h') { // print some help
            Serial.println("h for help");
            Serial.println("r for dump all registers");
            Serial.println("E to enable encryption");
            Serial.println("e to disable encryption");
            Serial.println("p for promiscuous mode");
            Serial.println("d to dump the flash area");
            Serial.println("D to delete the flash area");
            Serial.println("i to show the device id");
            Serial.println("t to display the radio temperature");
            Serial.println("z toggle echo on/off");
        }
        if (input == 'r') //d=dump all register values
            radio.readAllRegs();
        if (input == 'E') { //E=enable encryption
            radio.encrypt(ENCRYPTKEY);
            Serial.print("Encryption enabled with key "); Serial.println(ENCRYPTKEY);
        }
        if (input == 'e') { //e=disable encryption
            radio.encrypt(null);
            Serial.println("Encryption disabled");
        }
        if (input == 'p') {
            promiscuousMode = !promiscuousMode;
            radio.promiscuous(promiscuousMode);
            Serial.print("Promiscuous mode "); Serial.println(promiscuousMode ? "on" : "off");
        }
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
        if (input == 'D') {
            Serial.print("Deleting Flash chip ... ");
            flash.chipErase();
            while(flash.busy());
            Serial.println("DONE");
        }
        if (input == 'i') {
            Serial.print("DeviceID: ");
            word jedecid = flash.readDeviceId();
            Serial.println(jedecid, HEX);
        }
        if (input == 't')
        {
            byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
            byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
            Serial.print( "Radio Temp is ");
            Serial.print(temperature);
            Serial.print("C, ");
            Serial.print(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
            Serial.println('F');
        }
        if (input == 'z') { echoon = 1 - echoon; Serial.print("Echo "); Serial.println(echoon); }
    }
    if (radio.receiveDone()) {
        if (echoon) {
            Serial.print("#[");
            Serial.print(++packetCount);
            Serial.print(']');
            Serial.print('[');
            Serial.print(radio.SENDERID, DEC);
            Serial.print("] ");
            if (promiscuousMode) { Serial.print("to [");Serial.print(radio.TARGETID, DEC); Serial.print("] "); }
            Serial.print("Values received: \n");
            
            // Printing payload data.
            Payload* pay = (Payload*)radio.DATA;
            Serial.print("<NodeID "); Serial.print(pay->nodeId); Serial.print(" >\n");
            Serial.print("<Voltage 1 "); Serial.print(pay->v1); Serial.print(" >\n");
            Serial.print("<Voltage 2 "); Serial.print(pay->v2a); Serial.print(" >\n");
            Serial.print("<Voltage 3 "); Serial.print(pay->v3); Serial.print(" >\n");
            
            char testEndData[] = "Should be the computed data";
            
            if(radio.sendWithRetry(GATEWAYID, testEndData, sizeof(testEndData))) {
                Serial.print("GS sent data back\n");
            } else {
                Serial.print("nothing...\n");
            }
            
            // Sending ACK's where needed
            if (radio.ACKRequested()) {
                radio.sendACK();
                Serial.print("ACK sent\n");
            }
        } else {
            delay(10);
        }
        if (radio.ACKRequested()) {
            byte theNodeID = radio.SENDERID;
            radio.sendACK();
            if (echoon) Serial.print(" - ACK sent.");
            // When a node requests an ACK, respond to the ACK
            // and also send a packet requesting an ACK (every 3rd one only)
            // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
            if (ackCount++%3==0) {
                if (echoon) {
                    Serial.print(" Pinging node ");
                    Serial.print(theNodeID);
                    Serial.print(" - ACK...");
                } else {
                    delay(10);
                }
                delay(3); //need this when sending right after reception .. ?
                if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  { // 0 = only 1 attempt, no retries
                    if (echoon) { Serial.print("ok!"); } else { delay(10); }
                } else {
                    if (echoon) { Serial.print("nothing"); } else { delay(10); }
                }
            }
        }
        float v1;
        float v2;
        float v3;
        
        //Determination of Yaw Rotation (around z-axis; a.k.a.: "left/right rotation")
        diffLR  = v1-v3;
        diffLR1 = constrain(diffLR, -THRESHOLD_YAW, THRESHOLD_YAW); //Constraining the value of diff LR1;
        
        if (diffLR1>=0) {
        	// Maps the position of diffLR1 in the range of [0.0, 0.36] to an integer from [1, 6]
            int LR1 = map(diffLR1, 0.0, 0.36, 1, 6);
        }
        else {
        	// Maps the position of diffLR1 in the range of [-0.36, 0] to an integer from [-6, -1]
            int LR1 = map(diffLR1, -0.36, 0.0, -6, -1);
        }
        
        if(diffLR1 != diffLR) {
            if (LR1 > 0) {
              if (LR1 == 6) {
              	brightnessL = 255.0;
              } else {
              	brightnessL = 0.0;
              }
              brightnessR = LR1 * 40.0;
            } else if (LR1 < 0) {
              if (LR1 == -6) {
              	brightnessR = 255.0;
              } else {
              	brightnessR = 0.0;
              }
              brightnessL = abs(LR1) * 40.0;
            } else {
            	Serial.print("Error");
            }
        } else if (v2a>0.6 || v2b>0.6) {
        brightnessR = 255.0;
            brightnessL = 255.0;
        } else {
            brightnessR = 0.0;
            brightnessL = 0.0;
        }
        
        //Determination of Roll Rotation (around x-axis --> Up/Down Motion)
        diffUD = v2b-v2a;
        diffUD1 = constrain(diffUD, -THRESHOLD_ROLL, THRESHOLD_ROLL);
        int UD1 = map(abs(diffUD), 0.0, 0.36, 1, 6);
        
        if (diffUD>0) {
            switch(UD1) {
                case 1:
                    brightnessU = 40.0;
                    brightnessD = 0.0;
                    break;
                case 2:
                    brightnessU = 80.0;
                    brightnessD = 0.0;
                    break;
                case 3:
                    brightnessU = 120.0;
                    brightnessD = 0.0;
                    break;
                case 4:
                    brightnessU = 160.0;
                    brightnessD = 0.0;
                    break;
                case 5:
                    brightnessU = 200.0;
                    brightnessD = 0.0;
                    break;
                case 6:
                    brightnessU = 255.0;
                    brightnessD = 0.0;
                    break;
            }
        }
        else if (diffUD<0) {
            switch(UD1) {
                case 1:
                    brightnessD = 40.0;
                    brightnessU = 0.0;
                    break;
                case 2:
                    brightnessD = 80.0;
                    brightnessU = 0.0;
                    break;
                case 3:
                    brightnessD = 120.0;
                    brightnessU = 0.0;
                    break;
                case 4:
                    brightnessD = 160.0;
                    brightnessU = 0.0;
                    break;
                case 5:
                    brightnessD = 200.0;
                    brightnessU = 0.0;
                case 6:
                    brightnessD = 255.0;
                    brightnessU = 0.0;
                    break;
            }
        }
        else if (diffUD1 == diffUD) {
            brightnessU = 255.0;
            brightnessD = 255.0;
        }
    }
    
}

//if (echoon) Serial.println();
//Blink(LED,3);
//}

void Blink(byte PIN, int DELAY_MS) {
    pinMode(PIN, OUTPUT);
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
}




