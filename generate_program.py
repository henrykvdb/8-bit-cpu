from instruction_types import *
from instruction_set import *

"""
Pseudocode; fibonacci unrolled 2x to avoid swapping

reg[1] = 0
reg[2] = 1
(W = reg[2])

while True:
  print W
  W += reg[1]
  reg[1] = W

  print W
  W += reg[2]
  reg[2] = W
"""
def fibonacci():
    yield from reference_seq()

    while True:
        # reg[1] = 0
        yield get_instr_code("WLOAD", "IMM")
        yield "0"
        yield get_instr_code("WSTORE", "REGS[IMM]")
        yield "1"
        yield get_instr_code("WSTORE", "OUT_A")

        # reg[2] = 1
        yield get_instr_code("WLOAD", "IMM")
        yield "1"
        yield get_instr_code("WSTORE", "REGS[IMM]")
        yield "2"
        yield get_instr_code("WSTORE", "OUT_A")

        for _ in range(6): # TODO replace by JMP
            # reg[1] += reg[2]
            yield get_instr_code("WADD", "REGS[IMM]")
            yield "1"
            yield get_instr_code("WSTORE", "REGS[IMM]")
            yield "1"
            yield get_instr_code("WSTORE", "OUT_A")

            # reg[2] += reg[1]
            yield get_instr_code("WADD", "REGS[IMM]")
            yield "2"
            yield get_instr_code("WSTORE", "REGS[IMM]")
            yield "2"
            yield get_instr_code("WSTORE", "OUT_A")



def reference_seq_pc():
  for _ in range(8):
    yield get_instr_code("WLOAD", "PC_L")
    yield get_instr_code("WSTORE", "OUT_A")

def reference_seq():
  for i in reversed(range(10)):
    yield get_instr_code("WLOAD", "IMM")
    yield str(i)
    yield get_instr_code("WSTORE", "OUT_A")

def program_sp_testing():
  while True:
    yield get_instr_code("WLOAD", "PC_L")
    yield get_instr_code("WSTORE", "SP")
    yield get_instr_code("WSTORE", "REGS[SP]")   

def program_imm_testing():
  i = 0
  yield from reference_seq()
  while True:
    yield get_instr_code("WLOAD", "IMM")
    yield (2**i) # IMM
    i = (i + 1) % 8


#########################
# C++ HEADER GENERATION #
#########################

programs = [fibonacci()]
for program in programs:
    #for program in program_loop
    prog_str  = "// TODO -> 30kB ROM limit arduino. 65.5kB instructions...\n"
    prog_str += "const PROGMEM uint8_t instructions[16384] = {\n"

    for idx in range(16384):
        code = next(program)
        if type(code) is str: # IMM
          prog_str += f"{int(code):3}, // IMM\n"
        else:
          prog_str += f"{code:3}, // {get_instr_name(code)}\n"


    prog_str += "};\n"


    with open("EEPROM-arduino/instr-memory/instructions.hpp", "w") as f:
      f.write(prog_str)