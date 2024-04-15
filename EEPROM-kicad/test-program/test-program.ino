/* Programs the 17x8 Greenliant GLS29EE010 EEPROM using an Arduino Micro and 3 74HC595 shift registers.
 * KiCAD schematic of the EEPROM programmer can also be found in this repo.*/

#define EEPROM_D0 2 // data LSB
#define EEPROM_D7 9 // data MSB

#define SHIFT_DATA 10 // data to shift into shift register
#define SHIFT_CLK 11 // pos-edge clock for shift register
#define DFF_CLK 12 // pos-edge clock for DFF to set after filling shift register
#define SHIFT_CLR 13 // active-low async clear for shift registers

#define EEPROM_READ_EN A0 // active-low EEPROM read enable
#define EEPROM_WRITE_EN A1 // active-low EEPROM write enable
#define EEPROM_CHIP_EN A2 // active-low EEPROM chip enable

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(300);
  while (!Serial) { delay(10); }

  // *** WRITE THE DATA ***
  setPinsToDefaultForWriting();
  Serial.print("Programming EEPROM");
  Serial.flush();

  // Write the 1st page of data
  bypassSDP();
  for (int i = 0; i < 16; i++) // the first 2-16 bytes written in each page are skipped for some reason, so write 16 useless bytes
    writeEEPROM(0, 0xFF);
  writeEEPROM(0, 0xCA);
  writeEEPROM(1, 0XFE);
  writeEEPROM(2, 0xBA);
  writeEEPROM(3, 0xBE);
  endWriting(); // end the page write

  // Write the 2nd page of data
  bypassSDP();
  for (int i = 0; i < 16; i++) // the first 2-16 bytes written in each page are skipped for some reason, so write 16 useless bytes
    writeEEPROM(128, 0xFF);
  writeEEPROM(128, 0XDE);
  writeEEPROM(129, 0xAD);
  writeEEPROM(130, 0xBE);
  writeEEPROM(131, 0XEF);
  endWriting(); // end the page write
  
  Serial.println(" Done writing");
  Serial.flush();

  // *** CHECK WHAT WAS WRITTEN ***
  setPinsToDefaultForReading();
  print256Bytes();
  Serial.println("Done reading");
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

/* Shift in a new address and output it
 * Note: Call setPinsToDefaultForReading() before calling this function. */
void shiftAddress(unsigned long address)
{
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address));       // Outputs XXXX XXXX (bits 0-7)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 8));  // Outputs XXXX XXXX (bits 8-15)
  shiftOut(SHIFT_DATA, SHIFT_CLK, LSBFIRST, (address >> 16)); // Outputs 0000 000X (bits 16-23)

  // Show the new address to the EEPROM
  digitalWrite(DFF_CLK, HIGH);
  digitalWrite(DFF_CLK, LOW);
}

/* Reads from the EEPROM. 
 * Note: Call setPinsToDefaultForReading() before calling this function. */
byte readEEPROM(unsigned long address)
{
  // Set up the address
  shiftAddress(address);

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
  // End the previous write if there was one
  digitalWrite(EEPROM_WRITE_EN, HIGH);

  // Set up the address
  shiftAddress(address);
  
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