// Sample RFM69 sender/node sketch, with ACK and optional encryption
// Sends periodic messages of increasing length to gateway (id=1)
// It also looks for an onboard FLASH chip, if present
// Library and code by Felix Rusu - felix@lowpowerlab.com
// Get the RFM69 and SPIFlash library at: https://github.com/LowPowerLab/

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash

#define NODEID        2    //unique for each node on same network               <----------------------------- change this by group, avoid conflicting nodes
#define NETWORKID     100  //the same on all nodes that talk to each other      <----------------------------- keep it the same for all groups + beacon
#define GATEWAYID     1    //                                                   <----------------------------- must be 1
#define FREQUENCY     RF69_868MHZ  //                                           <----------------------------- must be like this because of antenna length
#define ENCRYPTKEY    "ae3535-practical" //                                     <----------------------------- exactly the same 16 characters/bytes on all nodes!
#define SERIAL_BAUD   115200
#define MASTERBEAT    1000  // leave it like this please (preferable increase it)

char ids[30];
int TRANSMITPERIOD = 1000;                         // transmit a packet to gateway so often (in ms) 
char payload[] = "123 ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // should be 30 characters exactly, see code below
char buff[20];
byte sendSize=0;
boolean requestACK = false;

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
RFM69 radio;

//---------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.print("Node running on network "); 
  Serial.print(NETWORKID); 
  Serial.print(" and ID "); 
  Serial.println(NODEID); 
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);
  //radio.setFrequency(919000000); //set frequency to some custom frequency
  Serial.print("Transmitter period "); Serial.print(TRANSMITPERIOD); Serial.println(" msec");
  char buff[50];
  sprintf(buff, "Transmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff); 
  if (flash.initialize()) {
    Serial.print("SPI Flash Init OK ... UniqueID (MAC): ");
    flash.readUniqueId();
    for (byte i=0; i<8; i++) {
      Serial.print(flash.UNIQUEID[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
    sprintf(ids,"%2X %2X %2X %2X %2X %2X %2X %2X",flash.UNIQUEID[0],flash.UNIQUEID[1],flash.UNIQUEID[2],flash.UNIQUEID[3],flash.UNIQUEID[4],flash.UNIQUEID[5],flash.UNIQUEID[6],flash.UNIQUEID[7]);
    int lids = strlen(ids);
    ids[lids] = 0;
  }
  else
    Serial.println("SPI Flash Init FAIL! (is chip present?)");
}

//---------------------------------------------------------------------------

long lastPeriod = 0;
void loop() {
  //process any serial input
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input >= 48 && input <= 57) { //[0,9]
      TRANSMITPERIOD = MASTERBEAT * (input-48);
      if (TRANSMITPERIOD == 0) TRANSMITPERIOD = MASTERBEAT;
      Serial.print("\nChanging delay to ");
      Serial.print(TRANSMITPERIOD);
      Serial.println("ms\n");
    }
    if (input == 'h') {
      Serial.println("[0-9] for transmitter repeat period (multiples of 1000ms)");
      Serial.println("r to dump the registers");
      Serial.println("E to enable encryption");
      Serial.println("e to disable encryption");
      Serial.println("d to dump flash area");
      Serial.println("X to erase the flash chip");
      Serial.println("i to show the device id");
    }
    if (input == 'r') { //d=dump register values
      radio.readAllRegs();
      Serial.println("dump register values");
    }
    if (input == 'E') { //E=enable encryption
      radio.encrypt(ENCRYPTKEY);
      Serial.print("enabled encryption with "); Serial.println(ENCRYPTKEY);
    }
    if (input == 'e') { //e=disable encryption
      radio.encrypt(null);
      Serial.println("disabled encryption");
    }
    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      uint16_t counter = 0;
      Serial.print("0-256: ");
      while(counter<=256) {
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      while(flash.busy());
      Serial.println();
    }
    if (input == 'X') {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    if (input == 'i') {
      Serial.print("DeviceID: ");
      word jedecid = flash.readDeviceId();
      Serial.println(jedecid, HEX);
    }
  }
  //check for any received packets
  if (radio.receiveDone()) {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++) Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    if (radio.ACKRequested()) {
      radio.sendACK();
      Serial.print(" - ACK sent");
    }
    Blink(LED,3);
    Serial.println();
  }
  //
  int currPeriod = millis()/TRANSMITPERIOD;
  if (currPeriod != lastPeriod) {
    lastPeriod=currPeriod;
    //send FLASH id
    if(sendSize==0) {
      Serial.print("Sending[0] ");
      if (radio.sendWithRetry(GATEWAYID, ids, strlen(ids))) { Serial.print(ids); Serial.print(" ok!"); }
      else Serial.print(" nothing...");
      //sendSize = (sendSize + 1) % 31;
    } else {
      Serial.print("Sending[");
      Serial.print(sendSize);
      Serial.print("]: ");
      for(byte i = 0; i < sendSize; i++) Serial.print((char)payload[i]);
      if (radio.sendWithRetry(GATEWAYID, payload, sendSize)) Serial.print(" ok!"); else Serial.print(" nothing...");
    }
    sendSize = (sendSize + 1) % 31;
    Serial.println();
    Blink(LED,3);
  }
}

//---------------------------------------------------------------------------

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
