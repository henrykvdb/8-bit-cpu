/* Programs the 17x8 Greenliant GLS29EE010 EEPROM using an Arduino Micro and 3 74HC595 shift registers.
 * KiCAD schematic of the EEPROM programmer can also be found in this repo.*/
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

// integer pow function (avoid double rounding)
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

// Convert full instruction -> EEPROM address
const unsigned long in_map_instr[8] = {
  // Pins for instruction code
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
  ipow(2, 9),
  ipow(2, 8),
  0L,0L,0L,0L
  };
const unsigned long in_map_flags[8] = {
  // Pins for ustep addresses
  ipow(2, 11),
  ipow(2, 10),
  0L,0L,0L,0L,0L,0L
  };

// Convert instruction step code -> instruction step control
const unsigned long out_map[8] = {
  ipow(2, 2), // ARGS_CODE 0
  ipow(2, 1), // ARGS_CODE 1
  ipow(2, 0), // ARGS_CODE 2
  ipow(2, 6), // ARGS_CODE 3 (select demux)
  ipow(2, 7), // ARGS_CODE 4 (select demux)
  ipow(2, 3), // ALU_CODE_0
  ipow(2, 4), // ALU_CODE_1
  ipow(2, 5)  // ALU_CODE_2
  };

unsigned long encode_instr_addr(unsigned int instr, unsigned int flags, unsigned int ustep){
  unsigned long encoded = 0;
  encoded += _encode(flags, in_map_flags);
  encoded += _encode(ustep, in_map_ustep);
  encoded += _encode(instr, in_map_instr);
  return encoded;
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

  unsigned long WRITE_SIZE = 128;
  byte data_buffer [WRITE_SIZE];
  unsigned long addr_buffer [WRITE_SIZE];

  for (unsigned int s_idx = 0; s_idx < 16; s_idx++){
    for (unsigned int f_idx = 0; f_idx < 4; f_idx++){
      Serial.println("WRITING PULSE: " + String(s_idx * 4 + f_idx) + "/64");
      // instruction inner loop to perform block writes
      for (unsigned int i_blk = 0; i_blk < 256 / WRITE_SIZE; i_blk++){
          // Pre-allocate the data buffer for block
          unsigned long base_addr = i_blk * WRITE_SIZE;
          for (unsigned int i_rep = 0; i_rep < WRITE_SIZE; i_rep++){
            unsigned int i_idx = base_addr + i_rep;
            data_buffer[i_rep] = _encode(pgm_read_byte_near(&decode_table[i_idx][f_idx][s_idx]), out_map);
            addr_buffer[i_rep] = encode_instr_addr(i_idx, f_idx, s_idx);
          }
          
          // Bypass write protection
          bypassSDP();
          for (unsigned int i = 0; i < 16; i++) // the first 2-16 bytes written in each page are skipped for some reason, so write 16 useless bytes
            writeEEPROM(base_addr, 0xFF);

          // Perform writes
          for (unsigned int i_rep = 0; i_rep < WRITE_SIZE; i_rep++){
            unsigned int i_idx = base_addr + i_rep;
            writeEEPROM(addr_buffer[i_rep], data_buffer[i_rep]);
          }

          // End page write
          endWriting();
      }
    }
  }

  // *** CHECK WHAT WAS WRITTEN ***
  Serial.println("Start verifying");
  setPinsToDefaultForReading();
  
  for (unsigned int s_idx = 0; s_idx < 16; s_idx++){
    for (unsigned int f_idx = 0; f_idx < 4; f_idx++){
      Serial.println("CHECKING PULSE: " + String(s_idx * 4 + f_idx) + "/64");
      for (unsigned int i_idx = 0; i_idx < 256; i_idx++){
        uint8_t data_exp =  _encode(pgm_read_byte_near(&decode_table[i_idx][f_idx][s_idx]), out_map);
        unsigned long addr_real = encode_instr_addr(i_idx, f_idx, s_idx);
        uint8_t data_real = readEEPROM(addr_real);
        if (data_exp != data_real){
            Serial.println("i=" + String(i_idx) + "  NOT OK");
            Serial.println("decode_table[" + String(i_idx) + "][" + String(f_idx) + "][" + String(s_idx) + "] =>  NOT OK");
            Serial.println(" exp = " + String(data_exp));
            Serial.println("real = " + String(data_real));
        }
      }
    }
  }

  Serial.println("Done verifying");
  Serial.flush();
}

void loop()
{
}

/* Bypass SDP by sending the appropriate 3-byte code to the appropriate addresses.
 * Does not finish the SDP write by sending EEPROM_WRITE_EN high. */
void bypassSDP()
{
  writeEEPROM(0x5555, 0xAA);
  writeEEPROM(0x2AAA, 0x55);
  writeEEPROM(0x5555, 0xA0);
}

/* Sets all the pins for reading from the EEPROM. */
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

/* Sets all the pins for writing to the EEPROM. */
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

/* Shift in a new address and output it => Need to toggle DFF CLK after to make visible
 * Note: Call setPinsToDefaultForReading() before calling this function. */
void shiftAddress(unsigned long address)
{
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address));       // Outputs XXXX XXXX (bits 0-7)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 8));  // Outputs XXXX XXXX (bits 8-15)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 16)); // Outputs 0000 000X (bits 16-23)
}

/* Reads from the EEPROM. 
 * Note: Call setPinsToDefaultForReading() before calling this function. */
byte readEEPROM(unsigned long address)
{
  // Set up the address
  shiftAddress(address);

  // Show the new address to the EEPROM
  digitalWrite(DFF_CLK, HIGH);
  digitalWrite(DFF_CLK, LOW);

  // Perform the read (reverse)
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--){
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

/* Start write to the EEPROM
 * Does not finish it in order to let the next writeEEPROM() call or an endWriting() finish it.
 * Note: Call setPinsToDefaultForReading() before calling this function. */
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
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++){
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

/* Prints the contents of the EEPROM at the first 256 addresses. No delays in this function.
 * 
 * Example output format:
 * 00000: 00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00
 * 00010: 00 00 00 00 00 00 00 00   00 00 00 00 00 00 00 00
 * ...
 * 
 * Note: Call setPinsToDefaultForReading() before calling this function. */
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
