/* Original source: https://github.com/Nickduino/Somfy_Remote/blob/master/Somfy_Remote.ino

   This sketch allows you to emulate a Somfy RTS or Simu HZ remote.
   If you want to learn more about the Somfy RTS protocol, check out https://pushstack.wordpress.com/somfy-rts-protocol/

   The rolling code will be stored in EEPROM, so that you can power the Arduino off.

   Easiest way to make it work for you:
    - Choose a remote number
    - Choose a starting point for the rolling code. Any unsigned int works, 1 is a good start
    - Upload the sketch
    - Long-press the program button of YOUR ACTUAL REMOTE until your blind goes up and down slightly
    - send 'p' to the serial terminal
  To make a group command, just repeat the last two steps with another blind (one by one)

  Then:
    - m, u or h will make it to go up
    - s make it stop
    - b, or d will make it to go down
    - you can also send a HEX number directly for any weird command you (0x9 for the sun and wind detector for instance)
*/

#include <EEPROM.h>
#define PORT_TX 14 // 14 of PORTD = DigitalPin 14

#define SYMBOL 604
#define HAUT 0x2
#define STOP 0x1
#define BAS 0x4
#define PROG 0x8
#define EEPROM_ADDRESS 0

const unsigned long remotes[] = {0x121301, 0x121302, 0x121303, 0x121304};

int eeprom_addresses[sizeof(remotes)] = {0};
unsigned int rollingCode[sizeof(remotes)]={0};

unsigned int newRollingCode = 101;       //<-- Change it!

byte frame[7];
byte checksum;

void BuildFrame(byte *frame, byte button);
void SendCommand(byte *frame, byte sync);


void setup() {
  Serial.begin(115200);
  
  pinMode(15, OUTPUT);  digitalWrite(15,HIGH);
  pinMode(16, OUTPUT);  digitalWrite(16,HIGH);
  pinMode(17, OUTPUT);  digitalWrite(17,HIGH);
  pinMode(18, OUTPUT);  digitalWrite(18,HIGH);
  pinMode(20, OUTPUT);  digitalWrite(20,HIGH);
  pinMode(21, OUTPUT);  digitalWrite(21,HIGH);
  pinMode(22, OUTPUT);  digitalWrite(22,HIGH);
  pinMode(23, OUTPUT);  digitalWrite(23,HIGH);
  pinMode(24, OUTPUT);  digitalWrite(24,HIGH);
  pinMode(25, OUTPUT);  digitalWrite(25,HIGH);
  pinMode(46, OUTPUT);  digitalWrite(46,HIGH);
  pinMode(47, OUTPUT);  digitalWrite(47,HIGH);
  pinMode(48, OUTPUT);  digitalWrite(48,HIGH);
  pinMode(49, OUTPUT);  digitalWrite(49,HIGH);
  pinMode(50, OUTPUT);  digitalWrite(50,HIGH);
  pinMode(51, OUTPUT);  digitalWrite(51,HIGH);
  pinMode(52, OUTPUT);  digitalWrite(52,HIGH);
  pinMode(53, OUTPUT);  digitalWrite(53,HIGH);

  
  pinMode(PORT_TX, OUTPUT); // DDRJ |= 1 << PORT_TX; // Pin 5 an output
  digitalWrite(PORT_TX,LOW); // PORTJ &= !(1 << PORT_TX); // Pin 5 LOW

  int eeprom_addresses_size = sizeof(eeprom_addresses[0]);
  

  for (int i = 0; i < sizeof(remotes) / sizeof(remotes[0]); i++) {
    eeprom_addresses[i] = i * eeprom_addresses_size;
    EEPROM.get(eeprom_addresses[i], rollingCode[i]);
    if (rollingCode[i] < newRollingCode) {
      EEPROM.put(eeprom_addresses[i], newRollingCode);
    }
    EEPROM.get(eeprom_addresses[i], rollingCode[i]);
    Serial.print("Simulated remote number : "); Serial.println(remotes[i], HEX);
    Serial.print("Current rolling code    : "); Serial.println(rollingCode[i]);
  }
}

void loop() {
  if (Serial.available() > 0) {
    int r = Serial.parseInt();
    //int r = 0;
    Serial.print("Input recieved:"); Serial.println(r);
    unsigned long remote_address = remotes[r];
    int eeprom_address = eeprom_addresses[r];
    char serie = (char)Serial.read();
    //char serie = 'p';
    Serial.println("");
    Serial.print("Remote : "); Serial.println(remote_address, HEX);
    if (serie == 'm' || serie == 'u' || serie == 'h') {
      Serial.println("Monte"); // Somfy is a French company, after all.
      BuildFrame(frame, remote_address, eeprom_address, HAUT);
    }
    else if (serie == 's') {
      Serial.println("Stop");
      BuildFrame(frame, remote_address, eeprom_address, STOP);
    }
    else if (serie == 'b' || serie == 'd') {
      Serial.println("Descend");
      BuildFrame(frame, remote_address, eeprom_address, BAS);
    }
    else if (serie == 'p') {
      Serial.println("Prog");
      BuildFrame(frame, remote_address, eeprom_address, PROG);
    }
    else {
      Serial.println("Custom code");
      BuildFrame(frame, remote_address, eeprom_address, serie);
    }

    Serial.println("");
    SendCommand(frame, 2);
    for (int i = 0; i < 2; i++) {
      SendCommand(frame, 7);
    }
  }
  delay(500);
}


void BuildFrame(byte *frame, unsigned long remote_address, int eeprom_address, byte button) {
  unsigned int code;
  EEPROM.get(eeprom_address, code);
  frame[0] = 0xA7; // Encryption key. Doesn't matter much
  frame[1] = button << 4;  // Which button did  you press? The 4 LSB will be the checksum
  frame[2] = code >> 8;    // Rolling code (big endian)
  frame[3] = code;         // Rolling code
  frame[4] = remote_address >> 16; // Remote address
  frame[5] = remote_address >>  8; // Remote address
  frame[6] = remote_address;       // Remote address

  Serial.print("Frame         : ");
  for (byte i = 0; i < 7; i++) {
    if (frame[i] >> 4 == 0) { //  Displays leading zero in case the most significant
      Serial.print("0");     // nibble is a 0.
    }
    Serial.print(frame[i], HEX); Serial.print(" ");
  }

  // Checksum calculation: a XOR of all the nibbles
  checksum = 0;
  for (byte i = 0; i < 7; i++) {
    checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
  }
  checksum &= 0b1111; // We keep the last 4 bits only


  //Checksum integration
  frame[1] |= checksum; //  If a XOR of all the nibbles is equal to 0, the blinds will
  // consider the checksum ok.

  Serial.println(""); Serial.print("With checksum : ");
  for (byte i = 0; i < 7; i++) {
    if (frame[i] >> 4 == 0) {
      Serial.print("0");
    }
    Serial.print(frame[i], HEX); Serial.print(" ");
  }


  // Obfuscation: a XOR of all the bytes
  for (byte i = 1; i < 7; i++) {
    frame[i] ^= frame[i - 1];
  }

  Serial.println(""); Serial.print("Obfuscated    : ");
  for (byte i = 0; i < 7; i++) {
    if (frame[i] >> 4 == 0) {
      Serial.print("0");
    }
    Serial.print(frame[i], HEX); Serial.print(" ");
  }
  Serial.println("");
  Serial.print("Rolling Code  : "); Serial.println(code);
  EEPROM.put(eeprom_address, code + 1); //  We store the value of the rolling code in the
  // EEPROM. It should take up to 2 adresses but the
  // Arduino function takes care of it.
}

void SendCommand(byte *frame, byte sync) {
  if (sync == 2) { // Only with the first frame.
    //Wake-up pulse & Silence
    digitalWrite(PORT_TX,HIGH); //PORTJ |= 1 << PORT_TX;
    delayMicroseconds(9415);
    digitalWrite(PORT_TX,LOW); //PORTJ &= !(1 << PORT_TX);
    //delayMicroseconds(89565);
    delayMicroseconds(65535);
    delayMicroseconds(24030);
  }

  // Hardware sync: two sync for the first frame, seven for the following ones.
  for (int i = 0; i < sync; i++) {
    digitalWrite(PORT_TX,HIGH); //PORTJ |= 1 << PORT_TX;
    delayMicroseconds(4 * SYMBOL);
    digitalWrite(PORT_TX,LOW); //PORTJ &= !(1 << PORT_TX);
    delayMicroseconds(4 * SYMBOL);
  }

  // Software sync
  digitalWrite(PORT_TX,HIGH); //PORTJ |= 1 << PORT_TX;
  delayMicroseconds(4550);
  digitalWrite(PORT_TX,LOW); //PORTJ &= !(1 << PORT_TX);
  delayMicroseconds(SYMBOL/2);


  //Data: bits are sent one by one, starting with the MSB.
  for (byte i = 0; i < 56; i++) {
    if (((frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
      digitalWrite(PORT_TX,LOW); //PORTJ &= !(1 << PORT_TX);
      delayMicroseconds(SYMBOL);
      digitalWrite(PORT_TX,HIGH); //PORTJ ^= 1 << PORT_TX;
      delayMicroseconds(SYMBOL);
    }
    else {
      digitalWrite(PORT_TX,HIGH); //PORTJ |= (1 << PORT_TX);
      delayMicroseconds(SYMBOL);
      digitalWrite(PORT_TX,LOW); //PORTJ ^= 1 << PORT_TX;
      delayMicroseconds(SYMBOL);
    }
  }

  digitalWrite(PORT_TX,LOW); //PORTJ &= !(1 << PORT_TX);
  delayMicroseconds(30415); // Inter-frame silence
}
