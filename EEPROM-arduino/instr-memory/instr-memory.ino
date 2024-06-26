/* Programs the 17x8 Greenliant GLS29EE010 EEPROM using an Arduino Micro and 3 74HC595 shift registers.
 * KiCAD schematic of the EEPROM programmer can also be found in this repo.*/
#include "instructions.hpp"

#define EEPROM_D0 2 // data LSB
#define EEPROM_D7 9 // data MSB

#define SHIFT_DATA 10 // data to shift into shift register
#define SHIFT_CLK 11 // pos-edge clock for shift register
#define DFF_CLK 12 // pos-edge clock for DFF to set after filling shift register
#define SHIFT_CLR 13 // active-low async clear for shift registers

#define EEPROM_READ_EN A0 // active-low EEPROM read enable
#define EEPROM_WRITE_EN A1 // active-low EEPROM write enable
#define EEPROM_CHIP_EN A2 // active-low EEPROM chip enable

uint32_t lpow(uint32_t base, uint16_t exp)
{
    uint32_t result = 1;
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

// Convert 16b PC -> EEPROM address
const uint32_t in_map[16] = {
  // PC_L
  lpow(2, 0),
  lpow(2, 1),
  lpow(2, 2),
  lpow(2, 3),
  lpow(2, 4),
  lpow(2, 5),
  lpow(2, 6),
  lpow(2, 7),
  // PC_H
  lpow(2,12),
  lpow(2,15),
  lpow(2,16),
  lpow(2,14),
  lpow(2,13),
  lpow(2, 8),
  lpow(2, 9),
  lpow(2,11)
  };


// Convert 16b PC -> EEPROM address
const uint32_t out_map[16] = {
  lpow(2, 3),
  lpow(2, 0),
  lpow(2, 4),
  lpow(2, 5),
  lpow(2, 7),
  lpow(2, 6),
  lpow(2, 1),
  lpow(2, 2),
  0L,0L,0L,0L,0L,0L,0L,0L
  };

// Convert 16b PC -> EEPROM address
uint32_t encode(uint16_t value, uint32_t mapping [16])
{
  uint32_t encoded = 0;
  uint16_t remain = value;
  uint16_t bit_idx = 0;
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
  
  const uint32_t instr_count = 65536;
  
  // Writing constants / variables
  const uint16_t print_count = 64;
  const uint16_t print_period = instr_count / print_count;
  uint16_t print_idx = 0;
  
  // Buffers
  uint32_t WRITE_SIZE = 128;
  byte data_buffer [WRITE_SIZE];
  uint32_t addr_buffer [WRITE_SIZE];

  for (uint16_t i_block = 0; i_block < instr_count / WRITE_SIZE; i_block++){
    uint32_t base_addr = i_block * WRITE_SIZE;

    // encode data
    for (uint16_t i = 0; i < WRITE_SIZE; i++){
      uint32_t in_addr = base_addr + i;
      data_buffer[i] = encode(pgm_read_byte_near(&instructions[in_addr%16384]), out_map);
      addr_buffer[i] = encode(in_addr, in_map);
      
      if (((in_addr + 1) % print_period) == 0){
        print_idx += 1;
        Serial.println("WRITING PULSE: " + String(print_idx) + "/" + String(print_count));
      }
    }


    bypassSDP();
    for (uint16_t i = 0; i < 16; i++) // the first 2-16 bytes written in each page are skipped for some reason, so write 16 useless bytes
      writeEEPROM(base_addr, 0xFF);
    
    for (uint16_t i = 0; i < WRITE_SIZE; i++){
      uint32_t addr = addr_buffer[i];
      byte data = data_buffer[i];
      writeEEPROM(addr, data);
    }

    endWriting(); // end the page write
  }

  // *** CHECK WHAT WAS WRITTEN ***
  Serial.println("Start verifying");
  setPinsToDefaultForReading();

  print_idx = 0;
  for (uint32_t idx = 0; idx < instr_count; idx++){
      if (((idx + 1) % print_period) == 0){
        print_idx += 1;
        Serial.println("CHECKING PULSE: " + String(print_idx) + "/" + String(print_count));
      }
      uint8_t data_exp =  encode(pgm_read_byte_near(&instructions[idx%16384]), out_map);
      uint32_t addr_real = encode(idx, in_map);
      uint8_t data_real = readEEPROM(addr_real);
      if (data_exp != data_real){
          Serial.println("instructions[" + String(idx) + "] =>  NOT OK");
          Serial.println(" exp = " + String(data_exp));
          Serial.println("real = " + String(data_real));
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
  for (uint16_t pin = EEPROM_D0; pin <= EEPROM_D7; pin++) // for each data pin
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
  for (uint16_t pin = EEPROM_D0; pin <= EEPROM_D7; pin++) // for each data pin
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
void shiftAddress(uint32_t address)
{
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address));       // Outputs XXXX XXXX (bits 0-7)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 8));  // Outputs XXXX XXXX (bits 8-15)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 16)); // Outputs 0000 000X (bits 16-23)
}

/* Reads from the EEPROM. 
 * Note: Call setPinsToDefaultForReading() before calling this function. */
byte readEEPROM(uint32_t address)
{
  // Set up the address
  shiftAddress(address);

  // Show the new address to the EEPROM
  digitalWrite(DFF_CLK, HIGH);
  digitalWrite(DFF_CLK, LOW);

  // Perform the read (reverse)
  byte data = 0;
  for (uint16_t pin = EEPROM_D7; pin >= EEPROM_D0; pin--){
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

/* Start write to the EEPROM
 * Does not finish it in order to let the next writeEEPROM() call or an endWriting() finish it.
 * Note: Call setPinsToDefaultForReading() before calling this function. */
void writeEEPROM(uint32_t address, byte data)
{
  // Set up the address
  shiftAddress(address);

  // End the previous write if there was one
  digitalWrite(EEPROM_WRITE_EN, HIGH);

  // Show the new address to the EEPROM
  digitalWrite(DFF_CLK, HIGH);
  digitalWrite(DFF_CLK, LOW);
  
  // Set up the data
  for (uint16_t pin = EEPROM_D0; pin <= EEPROM_D7; pin++){
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
  uint32_t baseAddr;

  byte data[16];
  for (baseAddr = 0UL; baseAddr < 256UL; baseAddr += 16UL) // for every 16 addresses in the EEPROM
  {
    for (uint16_t offset = 0U; offset < 16U; offset++) // for each address within the current set of 16 addresses
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
