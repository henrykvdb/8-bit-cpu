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

#define EEPROM_D0 2 // data LSB
#define EEPROM_D7 9 // data MSB

#define SHIFT_DATA 10 // data to shift into shift register
#define SHIFT_CLK 11 // pos-edge clock for shift register
#define DFF_CLK 12 // pos-edge clock for DFF to set after filling shift register
#define SHIFT_CLR 13 // active-low async clear for shift registers

#define EEPROM_READ_EN A0 // active-low EEPROM read enable
#define EEPROM_WRITE_EN A1 // active-low EEPROM write enable
#define EEPROM_CHIP_EN A2 // active-low EEPROM chip enable

/*Display mappings:
EEPROM_D0 = /           => 1
EEPROM_D1 = BOTTOM_LEFT => 2
EEPROM_D2 = BOTTOM      => 4
EEPROM_D3 = TOP_RIGHT   => 8
EEPROM_D4 = TOP         => 16
EEPROM_D5 = TOP_LEFT    => 32
EEPROM_D6 = MIDDLE      => 64
EEPROM_D7 = BOTTOM_RIGHT=> 128*/
#define BOTTOM_LEFT  2
#define BOTTOM       4
#define TOP_RIGHT    8
#define TOP          16
#define TOP_LEFT     32
#define MIDDLE       64
#define BOTTOM_RIGHT 128

// Next, convert mappings to digits
const int DISP_VAL_0 = BOTTOM_LEFT | BOTTOM | BOTTOM_RIGHT | TOP_LEFT | TOP | TOP_RIGHT;
const int DISP_VAL_1 = BOTTOM_RIGHT | TOP_RIGHT;
const int DISP_VAL_2 = TOP | TOP_RIGHT | MIDDLE | BOTTOM_LEFT | BOTTOM;
const int DISP_VAL_3 = DISP_VAL_1 | TOP | MIDDLE | BOTTOM;
const int DISP_VAL_4 = DISP_VAL_1 | TOP_LEFT | MIDDLE;
const int DISP_VAL_5 = TOP | TOP_LEFT | MIDDLE | BOTTOM_RIGHT | BOTTOM;
const int DISP_VAL_6 = (DISP_VAL_0 - TOP_RIGHT) | MIDDLE;
const int DISP_VAL_7 = DISP_VAL_1 | TOP;
const int DISP_VAL_8 = DISP_VAL_0 | MIDDLE;
const int DISP_VAL_9 = (DISP_VAL_0 - BOTTOM_LEFT) | MIDDLE;
// Note: abcdef lowercase to distinguish B from 8
// -> skipped for now
//const int DISP_VAL_A = DISP_VAL_8 - BOTTOM;
//const int DISP_VAL_B = B;
//const int DISP_VAL_C =  |  |  |  |  | ;
//const int DISP_VAL_D =  |  |  |  |  | ;
//const int DISP_VAL_E =  |  |  |  |  | ;
//const int DISP_VAL_F =  |  |  |  |  | ;
const int DISP_VALS [10] = {DISP_VAL_0, DISP_VAL_1, DISP_VAL_2, DISP_VAL_3, DISP_VAL_4, DISP_VAL_5, DISP_VAL_6, DISP_VAL_7, DISP_VAL_8, DISP_VAL_9};

const unsigned long bus_pin_values[8] = {
  pow(2,10),
  pow(2,11),
  pow(2, 9),
  pow(2, 8),
  pow(2,13),
  pow(2,14),
  pow(2,16),
  pow(2,15)
  };

// Select which display to render
const unsigned long addr_offsets_disp[4] = {
  0,                    // display 0
  pow(2,7),             // display 1
  pow(2,12),            // display 2
  pow(2,7) + pow(2,12)  // display 3
  };

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(300);
  while (!Serial) { delay(10); }

  char buffer[40];
  for (int i = 0; i < 10; i++){
    sprintf(buffer, "ENCODING %d=>%d", i, DISP_VALS[i]);
    Serial.println(buffer);
  }

  // *** WRITE THE DATA ***
  setPinsToDefaultForWriting();
  Serial.println("Programming EEPROM");
  Serial.flush();

  for (int i = 0; i < 256; i++){
    unsigned long decoded = 0;

    // Decode into an adress common to all displays
    int remain = i;
    int bus_pin = 0;
    while(remain > 0){
      if (remain & 1){
        decoded += bus_pin_values[bus_pin];
      }
      bus_pin += 1;
      remain = remain >> 1;
    }

    // Print what is being written
    //sprintf(buffer, "i=%d:%d (%d) => c%lu", i, i%10, DISP_VALS[i%10], decoded);
    //Serial.println(buffer);
    //Serial.flush();

    // Add display offset and program
    bool encountered_digit = false; // dont print leading 0
    for (int display_idx = 3; display_idx >= 0; display_idx--){
      //First 2-16 bytes written in each page are skipped, write junk
      bypassSDP();
      for (int i = 0; i < 16; i++)
        writeEEPROM(decoded, 0xFF);
      
      if (display_idx == 0){
        // always print
        int digit = i % 10;
        writeEEPROM(decoded, DISP_VALS[digit]);
      } else{
        unsigned long decoded_offset = addr_offsets_disp[display_idx];
        unsigned int power = pow(10, display_idx); 
        int digit = (i / power) % 10;
        if (digit > 0 || encountered_digit){
          // Non zero or zero but not leading
          writeEEPROM(decoded + decoded_offset, DISP_VALS[digit]);
          encountered_digit = true;
        } else{
          // Don't display anything, leading zero
          writeEEPROM(decoded + decoded_offset, 0);
        }

      }

      // Close the write
      endWriting();
    }

  }

  // *** CHECK WHAT WAS WRITTEN ***
  Serial.println("Start reading");
  Serial.flush();
  setPinsToDefaultForReading();
  byte data0 = readEEPROM(0);
  Serial.println(data0);
  byte data1 = readEEPROM(bus_pin_values[0]);
  Serial.println(data1);

  //print256Bytes();
  Serial.println("Done reading");
  Serial.flush();
}

void loop()
{
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