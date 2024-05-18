from instruction_types import *
from instruction_set import *

program = [
  get_instr_code("WLOAD", "IMM"),
  0, # replaced by IMM
  get_instr_code("WSTORE", "OUT_A")
]


prog_str  = "// TODO -> 30kB ROM limit arduino. 65.5kB instructions...\n"
prog_str += "const PROGMEM uint8_t instructions[16384] = {"

for idx in range(16384):
  prog_str += "\n"
  prog_idx = idx % len(program)
  code = program[prog_idx]
  code_name = get_instr_name(code)
  
  if prog_idx == 1:
    imm = idx % 256
    prog_str += f"{imm}, // IMM"
  else:
    prog_str += f"{code}, // {code_name}"


prog_str += "\n};\n"


with open("EEPROM-arduino/instr-memory/instructions.hpp", "w") as f:
  f.write(prog_str)