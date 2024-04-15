/*
 * Programs the 17x8 Greenliant GLS29EE010 EEPROM using an Arduino Micro and 3 74HC595 shift registers.
 * 
 * The following control signals for the shift registers are hardwired to the Arduino:
 * SHIFT_DATA (SER)
 * SHIFT_CLK (SRCLK)
 * DFF_CLK (RCLK)
 * SHIFT_CLR (SRCLR#)
 * 
 * The following control signals for the EEPROM are hardwired to the Arduino:
 * EEPROM_READ_EN (OE#)
 * EEPROM_WRITE_EN (WE#)
 * 
 * The shift registers are used for the address. The bottom 17 outputs are used.
 * The top 7 outputs are filled with 0s but not used.
 * Shifts data into the shift registers in LSB first format.
 * The lowest bit (QH on the lowest shift register) is connected to A0 (LSB) on the EEPROM.
 * The highest bit (QH (not QA b/c only need 1 bit) on the highest shift register) is connected to A16 (MSB) on the EEPROM.
 * The highest shift register's QA through QG are not connected to anything.
 * 
 * The 8 data lines are hardwired between the Arduino Micro and the EEPROM.
 * Lower Arduino pin number is used for LSB in the EEPROM.
 * Higher Arduino pin number is used for MSB in the EEPROM.
 */
#include "decode-table.hpp"

#define EEPROM_D0 2 // data LSB
#define EEPROM_D7 9 // data MSB

#define SHIFT_DATA 10 // data to shift into shift register
#define SHIFT_CLK 11 // pos-edge clock for shift register
#define DFF_CLK 12 // pos-edge clock for DFF to set after filling shift register
#define SHIFT_CLR 13 // active-low async clear for shift registers

#define EEPROM_READ_EN A0 // active-low EEPROM read enable
#define EEPROM_WRITE_EN A1 // active-low EEPROM write enable
#define EEPROM_CHIP_EN A2 // active-low EEPROM chip enable

int ipow(int base, int exp)
{
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return result;
}

// Convert instruction -> EEPROM address
const unsigned long in_map_instr[8] = {
  // Pins for 
  ipow(2, 0),
  ipow(2, 1),
  ipow(2, 2),
  ipow(2, 3),
  ipow(2, 4),
  ipow(2, 5),
  ipow(2, 6),
  ipow(2, 7)
  };
const unsigned long in_map_ustep[8] = {
  // Pins for ustep addresses
  ipow(2, 14),
  ipow(2, 13),
  ipow(2, 8),
  ipow(2, 9),
  0L,0L,0L,0L
  };
const unsigned long in_map_flags[8] = {
  // Pins for ustep addresses
  ipow(2, 11),
  ipow(2, 10),
  0L,0L,0L,0L,0L,0L
  };


// Convert 16b PC -> EEPROM address
const unsigned long out_map[8] = {
  ipow(2, 6), // ARGS_CODE 0
  ipow(2, 0), // ARGS_CODE 1
  ipow(2, 1), // ARGS_CODE 2
  ipow(2, 2), // ARGS_CODE 3
  ipow(2, 3), // ALU_CODE_0
  ipow(2, 4), // ALU_CODE_1
  ipow(2, 5), // ALU_CODE_2
  ipow(2, 7)  // RST
  };

unsigned long encode_instr_addr(unsigned int flags, unsigned int ustep, unsigned int instr){
  unsigned long encoded = 0;
  encoded += _encode(flags, in_map_flags);
  encoded += _encode(ustep, in_map_ustep);
  encoded += _encode(instr, in_map_instr);
  return encoded;
}

unsigned long encode_instr_data(unsigned int rst /*1b*/, unsigned int alu_code /*3b*/, unsigned int args_code /*4b*/){
  unsigned long value = 0;
  value +=       rst << 7;
  value +=  alu_code << 4;
  value += args_code << 0;
  return _encode(value, out_map);
}

unsigned long _encode(unsigned int value, unsigned long mapping [8])
{
  unsigned long encoded = 0;
  unsigned int remain = value;
  int bit_idx = 0;
  while(remain > 0){
    if (remain & 1){
      encoded += mapping[bit_idx];
    }
    bit_idx += 1;
    remain = remain >> 1;
  }

  return encoded;
}


void setup()
{
  // put your setup code here, to run once:
  Serial.begin(300);
  while (!Serial) { delay(10); }

  // *** WRITE THE DATA ***
  setPinsToDefaultForWriting();
  Serial.println("Programming EEPROM");
  Serial.flush();

  //Serial.println(encode(ipow(2, 0), out_map));
  //Serial.println(encode(ipow(2, 1), out_map));
  //Serial.println(encode(ipow(2, 2), out_map));
  //Serial.println(encode(ipow(2, 3), out_map));
  //Serial.println(encode(ipow(2, 4), out_map));
  //Serial.println(encode(ipow(2, 5), out_map));
  //Serial.println(encode(ipow(2, 6), out_map));
  //Serial.println(encode(ipow(2, 7), out_map));

  bypassSDP();
  unsigned long addr = encode_instr_addr(0, 0, 0); // flags, ustp, instr
  for (unsigned int i = 0; i < 16; i++) // the first 2-16 bytes written in each page are skipped for some reason, so write 16 useless bytes
    writeEEPROM(addr, 0xFF);
  byte data = encode_instr_data(0, 7, 0); // rst alu_code, args_code
  
  writeEEPROM(addr, data);
  endWriting();

  /*unsigned long instr_count = 256;
  unsigned long WRITE_SIZE = 128;
  byte data_buffer [WRITE_SIZE];
  for (unsigned int i_block = 0; i_block < instr_count / WRITE_SIZE; i_block++){
    unsigned long base_addr = i_block * WRITE_SIZE;

    // encode data
    for (unsigned int i = 0; i < WRITE_SIZE; i++){
      unsigned long in_addr = base_addr + i;
      unsigned long addr = encode(ipow(2, in_addr), in_map);
      data_buffer[i] = encode(i, out_map);
    }


    bypassSDP();
    for (unsigned int i = 0; i < 16; i++) // the first 2-16 bytes written in each page are skipped for some reason, so write 16 useless bytes
      writeEEPROM(base_addr, 0xFF);
    
      Serial.println("Programming EEPROM");
    for (unsigned int i = base_addr; i < base_addr + WRITE_SIZE; i++){
      unsigned long addr = encode(i, in_map);

      if (i < 8) {
        byte data = encode(ipow(2, i), out_map);
        writeEEPROM(addr, data);
      } else {
        byte data = encode(i, out_map);
        writeEEPROM(addr, data);
      }

    }

    endWriting(); // end the page write
  }*/

  // *** CHECK WHAT WAS WRITTEN ***
  Serial.println("Start reading");
  setPinsToDefaultForReading();
  print256Bytes();
  Serial.println("Done reading");
  Serial.flush();
}

int i = 0;
int x = 0;
void loop()
{
  x += decode_table[i]; // avoid optimizing variable away
  i += 1;
}

/*
 * Gets past SDP by sending the appropriate 3-byte code to the appropriate addresses.
 * Does not finish the SDP write by sending EEPROM_WRITE_EN high.
 */
void bypassSDP()
{
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0xA0);
}

/*
 * Sets all the pins for reading from the EEPROM. Don't have to manually handle EEPROM_READ_EN for reading.
 * No delay in this function.
 */
void setPinsToDefaultForReading()
{
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) // for each data pin
  {
    pinMode(pin, INPUT);
  }

  pinMode(SHIFT_DATA, OUTPUT);
  digitalWrite(SHIFT_DATA, LOW);
  pinMode(SHIFT_CLK, OUTPUT);
  digitalWrite(SHIFT_CLK, LOW);
  pinMode(DFF_CLK, OUTPUT);
  digitalWrite(DFF_CLK, LOW);
  pinMode(SHIFT_CLR, OUTPUT);
  digitalWrite(SHIFT_CLR, HIGH);

  pinMode(EEPROM_READ_EN, OUTPUT);
  digitalWrite(EEPROM_READ_EN, LOW); // always read
  pinMode(EEPROM_WRITE_EN, OUTPUT);
  digitalWrite(EEPROM_WRITE_EN, HIGH);
  pinMode(EEPROM_CHIP_EN, OUTPUT);
  digitalWrite(EEPROM_CHIP_EN, LOW);
}

/*
 * Sets all the pins for writing to the EEPROM. Still have to manually handle EEPROM_WRITE_EN for writing.
 * No delay in this function.
 */
void setPinsToDefaultForWriting()
{
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) // for each data pin
  {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  pinMode(SHIFT_DATA, OUTPUT);
  digitalWrite(SHIFT_DATA, LOW);
  pinMode(SHIFT_CLK, OUTPUT);
  digitalWrite(SHIFT_CLK, LOW);
  pinMode(DFF_CLK, OUTPUT);
  digitalWrite(DFF_CLK, LOW);
  pinMode(SHIFT_CLR, OUTPUT);
  digitalWrite(SHIFT_CLR, HIGH);

  pinMode(EEPROM_READ_EN, OUTPUT);
  digitalWrite(EEPROM_READ_EN, HIGH);
  pinMode(EEPROM_WRITE_EN, OUTPUT);
  digitalWrite(EEPROM_WRITE_EN, HIGH);
  pinMode(EEPROM_CHIP_EN, OUTPUT);
  digitalWrite(EEPROM_CHIP_EN, LOW);
}

/*
 * Sets the shift registers values so they contain the address. Does not clock the DFF H/L to have the shift registers output the address.
 * No delay in this function.
 * 
 * Note: CALL setPinsToDefaultForReading() OR setPinsToDefaultForWriting() BEFORE CALLING THIS FUNCTION.
 * This function does not make sure the pins are set properly.
 * 
 * Note 2: CALL digitalWrite(DFF_CLK, HIGH) AND digitalWrite(DFF_CLK, LOW) after calling this function.
 * Otherwise the shift registers will still be outputting the previous address.
 */
void shiftAddress(unsigned long address)
{
  // address can have 17 bits of important info, but int is only 16 bits long, so have to use long.
  // long is 32 bits.
  
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address));       // Outputs XXXX XXXX (bits 0-7)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 8));  // Outputs XXXX XXXX (bits 8-15)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 16)); // Outputs 0000 000X (bits 16-23)
}

/*
 * Reads from the EEPROM. No delay in this function.
 * 
 * Note: MAKE SURE YOU CALL setPinsToDefaultForReading() BEFORE CALLING THIS FUNCTION.
 * This function does not make sure the pins are set properly.
 */
byte readEEPROM(unsigned long address)
{
  // Set up the address
  shiftAddress(address);
  digitalWrite(DFF_CLK, HIGH);
  digitalWrite(DFF_CLK, LOW);

  // Perform the read
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) // for each data pin in reverse
  {
    data = (data << 1) + digitalRead(pin);
  }

  return data;
}

/*
 * Starts a write to the EEPROM. Does not finish it in order to let either the next writeEEPROM() call
 * or an endWriting() call finish it. No delay in this function.
 * 
 * Note: MAKE SURE YOU CALL setPinsToDefaultForWriting() BEFORE CALLING THIS FUNCTION.
 * This function does not make sure the pins are set properly.
 * 
 * Note 2: MAKE SURE YOU CALL endWriting() WHEN YOU ARE DONE SETTING THE DATA FOR THE PAGE WRITE.
 * Otherwise only some data will get written (probably nothing or everything except the last piece
 * you put in) and EEPROM_WRITE_EN will stay active (low).
 */
void writeEEPROM(unsigned long address, byte data)
{
  // Set up the address
  shiftAddress(address);

  // End the previous write if there was one
  digitalWrite(EEPROM_WRITE_EN, HIGH);

  // Show the new address to the EEPROM
  digitalWrite(DFF_CLK, HIGH);
  digitalWrite(DFF_CLK, LOW);
  
  // Set up the data
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) // for each data pin
  {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }

  // Start the write
  digitalWrite(EEPROM_WRITE_EN, LOW);
}

/*
 * Finishes the page write by making EEPROM_WRITE_EN go high and then waiting 20ms for the write to happen.
 * Can start writing a new page once this function is done.
 */
void endWriting()
{
  digitalWrite(EEPROM_WRITE_EN, HIGH);
  delay(20);
}

/*
 * Prints the contents of the EEPROM at the first 256 addresses. No delays in this function.
 * 
 * Example output format:
 * 00000: 00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00
 * 00010: 00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00
 * ...
 * 
 * Note: MAKE SURE YOU CALL setPinsToDefaultForReading() BEFORE CALLING THIS FUNCTION.
 * This function does not make sure the pins are set properly.
 */
void print256Bytes()
{
  unsigned long baseAddr;

  byte data[16];
  for (baseAddr = 0UL; baseAddr < 256UL; baseAddr += 16UL) // for every 16 addresses in the EEPROM
  {
    for (unsigned int offset = 0U; offset < 16U; offset++) // for each address within the current set of 16 addresses
    {
      data[offset] = readEEPROM(baseAddr + offset);
    }

    char buf[80];
    sprintf(buf, "%05lx: %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
      baseAddr, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10],
      data[11], data[12], data[13], data[14], data[15]);
    // the %05lx above has an L, not a one

    Serial.println(buf);
    Serial.flush();
  }
}