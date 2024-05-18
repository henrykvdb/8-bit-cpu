from instruction_types import *
from instruction_set import *

# Make a list of the codes
codes = []
for i in range(256):
  table = instructions[i].generate_table()
  instr_codes = [[] for _ in range(4)]

  for flags in range(4):
    for step in range(16):
      instr_codes[flags].append(table[flags][step].code())
  
  codes.append(instr_codes)

# Sanity check
assert len(codes) == 256
for i in range(256):
  assert len(codes[i]) == 4
  for flags in range(4):
    assert len(codes[i][flags]) == 16

def stringify_list(lst, newline=False):
  newline_str = "\n\t" if newline else ""
  return newline_str + "{" + ", ".join(str(x) for x in lst) + "}"

# Write table
tmp0 = [[stringify_list(x, newline=True) for x in nested] for nested in codes]
tmp1 = [stringify_list(x, newline=True) for x in tmp0]
codes_str = "const PROGMEM uint8_t decode_table[256][4][16] = "
codes_str += stringify_list(tmp1, newline=True) + ";"

with open("EEPROM-arduino/instr-to-control/decode-table.hpp", "w") as f:
  f.write(codes_str)