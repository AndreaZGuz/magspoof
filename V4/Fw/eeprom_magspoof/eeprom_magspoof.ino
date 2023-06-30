/************************************************************
  Magspoof V4
  Modified version by Eduardo Contreras
  for Electronic Cats
  
  Original code by Samy Kamk  r (http://samy.pl/magspoof/)
  
  This example demonstrates how to use Magspoof v3 by Electronic Cats
  https://github.com/ElectronicCats/magspoof
  
  Development environment specifics:
  IDE: Arduino 1.8.9
  Hardware Platform:
  Magspoof V4
  
  This code is beerware; if you see me (or any other Electronic Cats
  member) at the local, and you've found our code helpful,
  please buy us a round!
  Distributed as-is; no warranty is given.
*/


#define PINS_PORT 1
#define PIN_A_BIT 4
#define PIN_B_BIT 5

#define BUTTON_PIN 11
#define LED 34
#define CLOCK_US 500

#define BETWEEN_ZERO 53 // 53 zeros between track1 & 2

#define TRACKS 2

uint8_t writePtr = 0;
uint8_t writeData = 0;

//This is a fairly large array, store it in external memory with keyword __xdata
__xdata char recvStr[64];
uint8_t recvStrPtr = 0;
bool stringComplete = false;
uint16_t echoCounter = 0;

// consts get stored in flash as we don't adjust them
char tracks[2][64]= {
"%B123456781234567^LASTNAME/FIRST^YYMMSSSDDDDDDDDDDDDDDDDDDDDDDDDD?\0", // Track 1
";123456781234567=112220100000000000000?\0" // Track 2
};

char revTrack[41];

int sublen[] = {
  32, 48, 48 };

int bitlen[] = {
  7, 5, 5 };

unsigned int curTrack = 0;
uint8_t dir;

void blink(uint8_t pin, int msdelay, int times){
  for (uint8_t i = 0; i < times; i++){
    digitalWrite(pin, HIGH);
    delay(msdelay);
    digitalWrite(pin, LOW);
    delay(msdelay);
  }
}

// send a single bit out
void playBit(int sendBit){
  dir ^= 1;
  digitalWriteFast(PINS_PORT,PIN_A_BIT, dir);
  digitalWriteFast(PINS_PORT,PIN_B_BIT, !dir);
  delayMicroseconds(CLOCK_US);

  if (sendBit){
    dir ^= 1;
    digitalWriteFast(PINS_PORT,PIN_A_BIT, dir);
    digitalWriteFast(PINS_PORT,PIN_B_BIT, !dir);
  }
  delayMicroseconds(CLOCK_US);

}

// when reversing
void reverseTrack(int track){
  int i = 0;
  track--; // index 0
  dir = 0;

  while (revTrack[i++] != '\0');
  i--;
  while (i--)
    for (int j = bitlen[track]-1; j >= 0; j--)
      playBit((revTrack[i] >> j) & 1);
}

// plays out a full track, calculating CRCs and LRC
void playTrack(int track){
  int tmp, crc, lrc = 0;
  track--; // index 0
  dir = 0;

  // enable H-bridge and LED
  //digitalWrite(ENABLE_PIN, HIGH);

  // First put out a bunch of leading zeros.
  for (int i = 0; i < 25; i++)
    playBit(0);

  //
  for (int i = 0; tracks[track][i] != '\0'; i++)
  {
    crc = 1;
    tmp = tracks[track][i] - sublen[track];

    for (int j = 0; j < bitlen[track]-1; j++)
    {
      crc ^= tmp & 1;
      lrc ^= (tmp & 1) << j;
      playBit(tmp & 1);
      tmp >>= 1;
    }
    playBit(crc);
  }

  // finish calculating and send last "byte" (LRC)
  tmp = lrc;
  crc = 1;
  for (int j = 0; j < bitlen[track]-1; j++)
  {
    crc ^= tmp & 1;
    playBit(tmp & 1);
    tmp >>= 1;
  }
  playBit(crc);

  // if track 1, play 2nd track in reverse (like swiping back?)
  if (track == 0)
  {
    // if track 1, also play track 2 in reverse
    // zeros in between
    for (int i = 0; i < BETWEEN_ZERO; i++)
      playBit(0);

    // send second track in reverse
    reverseTrack(2);
  }

  // finish with 0's
  for (int i = 0; i < 5 * 5; i++)
    playBit(0);

  digitalWriteFast(PINS_PORT,PIN_A_BIT, LOW);
  digitalWriteFast(PINS_PORT,PIN_B_BIT, LOW);
}

// stores track for reverse usage later
void storeRevTrack(int track){
  int i, tmp, crc, lrc = 0;
  track--; // index 0
  dir = 0;

  for (i = 0; tracks[track][i] != '\0'; i++)
  {
    crc = 1;
    tmp = tracks[track][i] - sublen[track];

    for (int j = 0; j < bitlen[track]-1; j++)
    {
      crc ^= tmp & 1;
      lrc ^= (tmp & 1) << j;
      tmp & 1 ?
        (revTrack[i] |= 1 << j) :
        (revTrack[i] &= ~(1 << j));
      tmp >>= 1;
    }
    crc ?
      (revTrack[i] |= 1 << 4) :
      (revTrack[i] &= ~(1 << 4));
  }

  // finish calculating and send last "byte" (LRC)
  tmp = lrc;
  crc = 1;
  for (int j = 0; j < bitlen[track]-1; j++)
  {
    crc ^= tmp & 1;
    tmp & 1 ?
      (revTrack[i] |= 1 << j) :
      (revTrack[i] &= ~(1 << j));
    tmp >>= 1;
  }
  crc ?
    (revTrack[i] |= 1 << 4) :
    (revTrack[i] &= ~(1 << 4));

  i++;
  revTrack[i] = '\0';
}

void s_print(char *s) {
  for(int i = 0; i < 64 ; i++) {
      if(s[i] == '?')break;
      USBSerial_print(s[i]);
    }
  USBSerial_println();  
}

void dumpEEPROM() {
  USBSerial_println("DataFlash Dump:");
  for (uint8_t i = 0; i < 64; i++) {
    char eepromData = eeprom_read_byte(i);
    USBSerial_print(eepromData);
  }
  USBSerial_println();
  USBSerial_flush();
}

void setup(){
  Serial0_begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED,LOW);

  pinModeFast(PINS_PORT,PIN_A_BIT, OUTPUT);
  pinModeFast(PINS_PORT,PIN_B_BIT, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (uint8_t i = 0; i < 64; i++) {
    char eepromData = eeprom_read_byte(i);
    tracks[0][i] = eepromData;
    USBSerial_print(tracks[0][i]);
    if(tracks[0][i] == '?')break;
  }
  
  USBSerial_println();
  USBSerial_flush();

  

  storeRevTrack(TRACKS);
  USBSerial_println("MagSpoof test");

}

void loop(){
    if (digitalRead(BUTTON_PIN) == 0){
      USBSerial_println("MagSpoof");
      digitalWrite(LED,HIGH);
      playTrack(1 + (curTrack++ % 2));
      delay(500);
      digitalWrite(LED,LOW);
      s_print(tracks[0]);
      s_print(tracks[1]);
      delay(1000);
    }

  while (USBSerial_available()) {
    char serialChar = USBSerial_read();
    if ((serialChar == '\n') || (serialChar == '\r') ) {
      recvStr[recvStrPtr] = '\0';
      if (recvStrPtr > 0) {
        stringComplete = true;
        break;
      }
    } else {
      recvStr[recvStrPtr] = serialChar;
      recvStrPtr++;
      if (recvStrPtr == 127) {
        recvStr[recvStrPtr] = '\0';
        stringComplete = true;
        break;
      }
    }
  }

  if (stringComplete) {
    
    if(recvStr[0] == '%') {

      USBSerial_println("HELL to EEPROM");
      //strcpy(tracks[0], recvStr);

      for(uint8_t i = 0; i < 64 ; i++) {
        eeprom_write_byte(i, recvStr[i]);
        if(recvStr[i] == '?'){USBSerial_println("? found");break;}
      }
      dumpEEPROM();  
    }

    USBSerial_print("ECHO:");
    USBSerial_println(recvStr);

    s_print(tracks[0]);
    s_print(tracks[1]);  
    USBSerial_println();

    USBSerial_flush();
    stringComplete = false;
    recvStrPtr = 0;

    echoCounter++;
    USBSerial_print("echo count: ");
    USBSerial_println(echoCounter);
    USBSerial_flush();
  }
}
