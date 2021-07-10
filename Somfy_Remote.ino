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
    - The array id of the remote address
    - m, u or h will make it to go up
    - s make it stop
    - b, or d will make it to go down
    - r will resend the last frame (like keep pressing the button)
    - you can also send a HEX number directly for any weird command you (0x9 for the sun and wind detector for instance)
*/

#include <EEPROM.h>
#define PORT_TX 14 // 14 of PORTD = DigitalPin 14

#define SYMBOL 604
#define HAUT 0x2
#define STOP 0x1
#define BAS 0x4
#define PROG 0x8

// Array of IO pins providing power to the RFLink shield
const byte rflink_power_port[] = {15, 16, 17, 18, 20, 21, 22, 23, 46 , 47 , 48 , 49, 50, 51 , 52, 53};  

// Array of simulated Somfy RTS addresses
const unsigned long remotes[] = {0x121301, 0x121302, 0x121303, 0x121304, 0x121305, 0x121306, 0x121307, 0x121308, 0x121309};

// Dynamic arrays to store and maintain the rolling codes in the system eeprom
int eeprom_addresses[sizeof(remotes)] = {0};
unsigned int rollingCode[sizeof(remotes)] = {0};

// Program these IO pins to simulate up/down commands
const byte io_port_down[] = {28, 30, 32, 34, 36, 38, 40, 42, 44};
const byte io_port_up[]   = {29, 31, 33, 35, 37, 39, 41, 43, 45};

byte io_port_down_state[sizeof(io_port_down)] = {0};
byte io_port_up_state[sizeof(io_port_up)] = {0};

// Initial rolling code for new addresses
unsigned int newRollingCode = 1;

byte last_frame[sizeof(remotes)][7];
byte frame[7];

void SendCommand(byte *frame, byte sync);
void SendFrame (byte *frame, int remote_id, int repeat_count);
void BuildFrame(byte *frame, int remote_id, byte button);


void setup() {
  Serial.begin(115200);

  for (int i = 0; i < sizeof(rflink_power_port) / sizeof(rflink_power_port[0]); i++) {
    pinMode(rflink_power_port[i], OUTPUT);
    digitalWrite(rflink_power_port[i], HIGH);
  }

  for (int i = 0; i < sizeof(io_port_down) / sizeof(io_port_down[0]); i++) {
    pinMode(io_port_down[i], INPUT_PULLUP);
    pinMode(io_port_up[i], INPUT_PULLUP);
    digitalWrite(io_port_down[i], HIGH);
    digitalWrite(io_port_up[i], HIGH);
  }

  pinMode(PORT_TX, OUTPUT);
  digitalWrite(PORT_TX, LOW);

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
  int remote_id = -1;
  int repeat_count = 10; // by default resend every frame 2 times to make sure it's received correctly.

  if (Serial.available() > 0) {
    remote_id = Serial.parseInt();
    Serial.print("Input recieved:"); Serial.println(remote_id);
    char serie = (char)Serial.read();
  
    if (serie == 'm' || serie == 'u' || serie == 'h') {
      Serial.println("Monte"); // Somfy is a French company, after all.
      BuildFrame(frame, remote_id, HAUT);
    }
    else if (serie == 's') {
      Serial.println("Stop");
      BuildFrame(frame, remote_id, STOP);
    }
    else if (serie == 'b' || serie == 'd') {
      Serial.println("Descend");
      BuildFrame(frame, remote_id, BAS);
    }
    else if (serie == 'p') {
      Serial.println("Prog");
      BuildFrame(frame, remote_id, PROG);
      repeat_count = 5; // Increase repeat_count to allow de-registration
    }
    //else if (serie == 'r') { //repeat last frame
    //  Serial.println("Repeating last frame");
    //  frame[0] = last_frame[remote_id][0];
    //  frame[1] = last_frame[remote_id][1];
    //  frame[2] = last_frame[remote_id][2];
    //  frame[3] = last_frame[remote_id][3];
    //  frame[4] = last_frame[remote_id][4];
    //  frame[5] = last_frame[remote_id][5];
    //  frame[6] = last_frame[remote_id][6];
    //}
    else {
      Serial.println("Custom code");
      BuildFrame(frame, remote_id, serie);
    }
  
    SendFrame(frame, remote_id, repeat_count);
  } else {
    boolean state_changed = false;
    
    for (int remote_id = 0; remote_id < sizeof(io_port_down) / sizeof(io_port_down[0]) && remote_id < sizeof(remotes) / sizeof(remotes[0]); remote_id++) {
      if (digitalRead(io_port_down[remote_id]) == LOW) {
        if (io_port_down_state[remote_id] == 0){
          Serial.println("Descend");
          BuildFrame(frame, remote_id, BAS);
          delay(500);
          if (digitalRead(io_port_down[remote_id]) == LOW) {
            io_port_down_state[remote_id] = 1;
            SendFrame(frame, remote_id, repeat_count);
          }
        }
      } else {
        if (io_port_down_state[remote_id] == 1) {
          io_port_down_state[remote_id] = 0;
          state_changed = true;
        }
      }
      if (digitalRead(io_port_up[remote_id]) == LOW) {
        if (io_port_up_state[remote_id] == 0){
          Serial.println("Monte"); // Somfy is a French company, after all.
          BuildFrame(frame, remote_id, HAUT);
          delay(500);
          if (digitalRead(io_port_up[remote_id]) == LOW) {
            io_port_up_state[remote_id] = 1;
            SendFrame(frame, remote_id, repeat_count);
          }
        }
      } else {
        if (io_port_up_state[remote_id] == 1) {
          io_port_up_state[remote_id] = 0;
          state_changed = true;
        }
      }

      // When the up or down pin switched from high to low we should send the stop signal.
      // TODO - Figure out that the somy motor already stopped. In that case we shouldn't sent a stop signal.      
      //if (state_changed && io_port_down_state[remote_id] == 0 && io_port_up_state[remote_id] == 0) {
      //  Serial.println("Stop");
      //  BuildFrame(frame, remote_id, STOP);
      //  state_changed = false;
      //  SendFrame(frame, remote_id, repeat_count);
      //}
    }
  }
}

void BuildFrame(byte *frame, int remote_id, byte button) {
  unsigned int code;
  unsigned long remote_address = remotes[remote_id];
  int eeprom_address = eeprom_addresses[remote_id];
 
  EEPROM.get(eeprom_address, code);
  frame[0] = 0xA7;          // Encryption key. Doesn't matter much
  frame[1] = button << 4;   // Which button did  you press? The 4 LSB will be the checksum
  frame[2] = code >> 8;     // Rolling code (big endian)
  frame[3] = code;          // Rolling code
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
  byte checksum = 0;
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
  EEPROM.put(eeprom_address, code + 1);
  // We store the value of the rolling code in the
  // EEPROM. It should take up to 2 adresses but the
  // Arduino function takes care of it.

}

void SendFrame (byte *frame, int remote_id, int repeat_count) {
  SendCommand(frame, 2);
  for (int i = 0; i < repeat_count; i++) {
    SendCommand(frame, 7);
  }

  // Save frame for later repeat (if needed).
  //last_frame[remote_id][0] = frame[0];
  //last_frame[remote_id][1] = frame[1];
  //last_frame[remote_id][2] = frame[2];
  //last_frame[remote_id][3] = frame[3];
  //last_frame[remote_id][4] = frame[4];
  //last_frame[remote_id][5] = frame[5];
  //last_frame[remote_id][6] = frame[6];

}

void SendCommand(byte *frame, byte sync) {
  if (sync == 2) { // Only with the first frame.
    //Wake-up pulse & Silence
    digitalWrite(PORT_TX, HIGH);
    delayMicroseconds(9415);
    digitalWrite(PORT_TX, LOW);

    delayMicroseconds(65535);
    delayMicroseconds(24030);
  }

  // Hardware sync: two sync for the first frame, seven for the following ones.
  for (int i = 0; i < sync; i++) {
    digitalWrite(PORT_TX, HIGH);
    delayMicroseconds(4 * SYMBOL);
    digitalWrite(PORT_TX, LOW);
    delayMicroseconds(4 * SYMBOL);
  }

  // Software sync
  digitalWrite(PORT_TX, HIGH);
  delayMicroseconds(4550);
  digitalWrite(PORT_TX, LOW);
  delayMicroseconds(SYMBOL / 2);


  //Data: bits are sent one by one, starting with the MSB.
  for (byte i = 0; i < 56; i++) {
    if (((frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
      digitalWrite(PORT_TX, LOW);
      delayMicroseconds(SYMBOL);
      digitalWrite(PORT_TX, HIGH);
      delayMicroseconds(SYMBOL);
    }
    else {
      digitalWrite(PORT_TX, HIGH);
      delayMicroseconds(SYMBOL);
      digitalWrite(PORT_TX, LOW);
      delayMicroseconds(SYMBOL);
    }
  }

  digitalWrite(PORT_TX, LOW);

  // Inter-frame silence
  delayMicroseconds(30415);
}
