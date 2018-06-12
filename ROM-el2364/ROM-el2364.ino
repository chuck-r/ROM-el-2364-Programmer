//EEPROMCE is always low!
const int EEPROMWE = 2;
const int EEPROMOE = 3;

const int ADDROE = 5;
const int ADDRCLK = 6;
const int ADDRSER = 7;

const int DATAOUTOE = 9;
const int DATAINOE = 4;
const int DATACLK = 10;
const int DATAOUTSER = 11;
const int DATAINSER = 8;

const int DEBUGBTN = 12;

const bool debug = false;
bool debugbutton = false;

enum Mode {
  READ = LOW,
  WRITE = HIGH,
  ERASE,
  VERIFY
};

Mode mode = READ;

void setup() {
  pinMode(ADDROE, OUTPUT);
  digitalWrite(ADDROE, HIGH);
  pinMode(ADDRCLK, OUTPUT);
  digitalWrite(ADDRCLK, LOW);
  pinMode(ADDRSER, OUTPUT);
  digitalWrite(ADDRSER, LOW);
  pinMode(DATAINOE, OUTPUT);
  digitalWrite(DATAINOE, LOW);
  pinMode(DATAOUTOE, OUTPUT);
  digitalWrite(DATAOUTOE, HIGH);
  pinMode(DATACLK, OUTPUT);
  digitalWrite(DATACLK, LOW);
  pinMode(DATAOUTSER, OUTPUT);
  digitalWrite(DATAOUTSER, LOW);
  pinMode(DATAINSER, INPUT_PULLUP);
  pinMode(EEPROMOE, OUTPUT);
  digitalWrite(EEPROMOE, HIGH);
  pinMode(EEPROMWE, OUTPUT);
  digitalWrite(EEPROMWE, HIGH);
  pinMode(DEBUGBTN, INPUT_PULLUP);
  //If the debug button is held at startup, enable debugging mode
  if( digitalRead(DEBUGBTN) == LOW )
  {
    debugbutton = true;
    while(digitalRead(DEBUGBTN) == LOW )
      continue;
  }
  char serialbuff[5];
  memset(serialbuff, 0x00, sizeof(serialbuff));
  Serial.begin(115200);
  //if( debugbutton )
    Serial.setTimeout(600000);
  //else
  //  Serial.setTimeout(30000);
  Serial.print("INIT");
  //Let's do some byte transfers!
}

void printMessage(String message)
{
  Serial.write(message.length()+2);
  Serial.println(message);
}

String printBinary(byte data)
{
  String temp = "";
  for( int i = 7; i >= 0; i-- )
  {
    if( data & (1 << i) )
      temp += "1";
    else
      temp += "0";
  }
  return temp;
}

void waitfordbg()
{
  if( debugbutton )
  {
    if( digitalRead(DEBUGBTN) == HIGH )
      while( digitalRead(DEBUGBTN) == HIGH );
    //Set serial timeout to 10 minutes
    unsigned long timeout = millis() + 500;
    while( digitalRead(DEBUGBTN) == LOW )
    {
      if( millis() > timeout )
        break;
    }
    delay(75);
  }
}

void debugBtnMessage(String message)
{
  if( debugbutton )
  {
    printMessage(message);
    waitfordbg();
  }
}

void setReadMode()
{
  if( mode == READ )
    return;
  //Disable shift register (write)
  digitalWrite(DATAOUTOE, HIGH);
  mode = READ;
}

void setWriteMode()
{
  if( mode == WRITE )
    return;
  //Disable shift-in register (read)
  digitalWrite(DATAINOE, LOW);
  mode = WRITE;
}

//Shift byte out to SN74HC595 (with tied data lines)
void shiftByteOut(byte data, bool noclock, int clkpin, int serpin, int oepin)
{
  if( oepin == DATAOUTOE )
    digitalWrite(DATAOUTOE, HIGH);
  else
    digitalWrite(oepin, HIGH);

  byte tmpdata = data;

  //Shift MSB-first
  for ( int i = 0; i < 8; i++ )
  {
    digitalWrite(serpin, tmpdata >> 7);
    digitalWrite(clkpin, HIGH);
    digitalWrite(clkpin, LOW);
    tmpdata = tmpdata << 1;
  }
  digitalWrite(serpin, LOW);
  //With tied clock lines, an additional clock pulse is necessary after shifting
  //the very last bit to all shift registers in order to output expected data.
  if(! noclock)
  {
    //Additional clock due to tied RCLK/SRCLK lines
    digitalWrite(clkpin, HIGH);
    digitalWrite(clkpin, LOW);
  }
  if( oepin == DATAOUTOE )
    digitalWrite(DATAOUTOE, LOW);
  else
    digitalWrite(oepin, LOW);
}

//Shift byte in from SN74HC165
byte shiftByteIn(int clkpin, int serpin, int latchpin, short bits = 8)
{
  byte input = 0;
  digitalWrite(clkpin, HIGH);
  if( latchpin == DATAINOE )
    digitalWrite(DATAINOE, LOW);
  else
    digitalWrite(latchpin, LOW);
  if( latchpin == DATAINOE )
    digitalWrite(DATAINOE, HIGH);
  else
    digitalWrite(latchpin, HIGH);
  //Data shift in MSB-first
  for ( int i = 0; i < bits; i++ )
  {
    digitalWrite(clkpin, HIGH);
    //Don't clock first, 74HC165 outputs first byte
    //as soon as latch goes high
    debugBtnMessage("Shift-in serial data: " + String(int(digitalRead(serpin))));
    input |= digitalRead(serpin) << ((bits-1)-i);
    //debugBtnMessage("input: " + printBinary(input));
    digitalWrite(clkpin, LOW);
  }
  digitalWrite(DATAINOE, LOW);
  return input;
}

//Write to address lines
void shiftAddress(unsigned short addr)
{
  shiftByteOut(byte((addr >> 8) & 0x00FF), true, ADDRCLK, ADDRSER, ADDROE);
  shiftByteOut(byte(addr & 0x00FF), false, ADDRCLK, ADDRSER, ADDROE);
  debugBtnMessage("Shifted out address: " + printBinary(addr >> 8) + printBinary(addr & 0xFF));
}

//Write to data lines
void shiftData(byte data)
{
  setWriteMode();
  shiftByteOut(data, false, DATACLK, DATAOUTSER, DATAOUTOE);
  debugBtnMessage("Shifted out data " + printBinary(data) + " out");
}

//Shift data lines in
//Arguments:
//bits: How many bits to shift in (useful in waitForToggleBit() so that cycles
//      aren't wasted shifting in bits we don't care about.)
byte shiftInData(short bits = 8)
{
  setReadMode();
  return shiftByteIn(DATACLK, DATAINSER, DATAINOE, bits);
}

//Never could get this to work properly, but waitForToggleBit() works, so...
/*void dataPoll(unsigned short addr, byte data)
{
  setReadMode();
  digitalWrite(EEPROMOE, LOW);
  __builtin_avr_delay_cycles(1);
  bool validdata = data & 0x80;
  while( true )
  {
    digitalWrite(EEPROMOE, LOW);
    byte addressread = shiftInData();
    addressread &= 0x80;
    digitalWrite(EEPROMOE, HIGH);
    if( addressread != validdata )
      continue;
    else
      break;
  }
}*/

//Implementation of "AC Load" from AT49F512 datasheet
void ACLoad(unsigned short addr, byte data)
{
  setWriteMode();
  digitalWrite(EEPROMOE, HIGH);
  shiftAddress(addr);
  digitalWrite(EEPROMWE, LOW);
  __builtin_avr_delay_cycles(2); //Delay an extra cycle to satisfy tWP = 90ns
  shiftData(data);
  digitalWrite(EEPROMWE, HIGH);
  __builtin_avr_delay_cycles(2); //Delay cycles to satisfy tWPH = 90ns
}

//Implementation of "AC Read" from AT49F512 datasheet
byte ACRead(unsigned short addr)
{
  setReadMode();
  digitalWrite(EEPROMWE, HIGH);
  shiftAddress(addr);
  digitalWrite(EEPROMOE, LOW);
  __builtin_avr_delay_cycles(2);
  debugBtnMessage("Check data in");
  byte input = shiftInData();
  digitalWrite(EEPROMOE, HIGH);
  return input;
}

byte writeAddress(unsigned short addr, byte data)
{
  if( debug )
    return 0x00;
  ACLoad(0x5555, 0xAA);
  ACLoad(0x2AAA, 0x55);
  ACLoad(0x5555, 0xA0);
  ACLoad(addr, data);
  digitalWrite(EEPROMOE, LOW);
  debugBtnMessage("Check address (" + printBinary((addr>>8)) + printBinary(addr & 0x00FF) + ") and data (" + printBinary(data) + ")");
  //dataPoll(addr, data);
  waitForToggleBit();
  return 0x00;
}

void eraseChip()
{
  if( debug )
    return;
  digitalWrite(EEPROMOE, HIGH);
  ACLoad(0x5555, 0xAA);
  ACLoad(0x2AAA, 0x55);
  ACLoad(0x5555, 0x80);
  ACLoad(0x5555, 0xAA);
  ACLoad(0x2AAA, 0x55);
  ACLoad(0x5555, 0x10);
  waitForToggleBit();
}

//This Atmel chip, during a program or erase cycle will implement a "toggle bit."
//Per the datasheet:
//"During a program or erase operation, successive attempts to read data from the device
//will result in I/O6 toggling between one and zero. Once the program cycle has completed,
//I/O6 will stop toggling and valid data will be read."
void waitForToggleBit()
{
  setReadMode();
  digitalWrite(EEPROMOE, LOW);
  //Store bit 7 (I/O 6)
  byte initial = shiftInData(2) & 0x40;
  while( true )
  {
    digitalWrite(EEPROMOE, HIGH);
    __builtin_avr_delay_cycles(3);
    digitalWrite(EEPROMOE, LOW);
    //Check bit 7 again
    byte nextbit = shiftInData(2) & 0x40;
    //If bit 7 is stable, the write/erase has finished.
    if( nextbit == initial )
      break;
    else
      initial = nextbit;
  }
}

void verifyRange(unsigned short startaddr, unsigned short endaddr)
{
  for ( unsigned short i = startaddr; i <= endaddr; i++ )
  {
    Serial.write(0x01);
    Serial.write(ACRead(i));
  }
}

unsigned short address = 0x0000;

void loop() {
  byte data;
  byte serialbuffer[6];
  memset(serialbuffer, 0x00, sizeof(serialbuffer));
  /*Protocol definition:
  <packet> ::= <length> <command> | <length> <command> <data>
  <data> ::= <address> | <address> <byte>
  <command> ::= 'R' | 'W' | 'E' comment <erase command>
  <byte> ::= <a single byte>
  <address> ::= <2-byte address>
  <length> ::= <1-byte length of packet (excluding length byte itself)>

  This allows two modes of operation:
  In the first mode (i.e. <length>R\x01\x00), the controlling application
  specifies all addresses (and, in write mode, the data to be stored). In the
  second mode (i.e. <length>R) bytes are read/written sequentially, using the
  address variable stored on the Arduino.

  If a program wanted to read 8 bytes starting at 0x0100, either of these
  command sets could be used:
  R\x01\x00
  R\x01\x01
  R\x01\x02
  R\x01\x03
  R\x01\x04
  R\x01\x05
  R\x01\x06
  R\x01\x07

  or

  R\x01\x00 //Read 7 sequential addresses
  R
  R
  R
  R
  R
  R
  R
  */

  while ( !Serial.available() )
    continue;

  Serial.readBytes(serialbuffer, 1);
  byte packetlen = serialbuffer[0];

  while ( Serial.available() != packetlen )
    continue;

  Serial.readBytes(serialbuffer, packetlen);

  //Read mode
  if ( serialbuffer[0] == 'R' )
  {
    setReadMode();
    //No address given
    if ( packetlen == 1 )
    {
      data = ACRead(address);
      Serial.write(0x01);
      Serial.write(data);
      //Serial.flush();
    }
    //Address given
    else
    {
      unsigned short packetaddress = serialbuffer[1] << 8;
      packetaddress += serialbuffer[2];
      address = packetaddress;
      data = ACRead(packetaddress);
      Serial.write(0x01);
      Serial.write(data);
    }
    address++;
  }
  //Write mode
  else if ( serialbuffer[0] == 'W' )
  {
    unsigned short retval;
    setWriteMode();
    //No address given
    if ( packetlen == 2 )
      retval = writeAddress(address, serialbuffer[1]);
    //Address given
    else
    {
      unsigned short packetaddress = (serialbuffer[1] << 8) & 0xFF00;
      packetaddress += serialbuffer[2];
      address = packetaddress;
      byte retval = writeAddress(packetaddress, serialbuffer[3]);
    }
    Serial.write(0x01);
    Serial.write(retval);
    address++;
  }
  //Erase mode
  else if ( serialbuffer[0] == byte('E') )
  {
    mode = ERASE;
    eraseChip();
    Serial.write(0x01);
    Serial.write(0x01);
  }
  //Verify mode (UNTESTED)
  else
  {
    mode = VERIFY;
    unsigned short startAddress = serialbuffer[1] << 8;
    startAddress += serialbuffer[2];
    unsigned short endAddress = serialbuffer[3] << 8;
    endAddress = serialbuffer[4];
    verifyRange(startAddress, endAddress);
  }
}
