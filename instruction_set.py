from instruction_types import *

instructions = []

def get_instr_code(name: str, arg: str):
  for idx, instr in enumerate(instructions):
    if instr.name == name and instr.arg == arg:
      return idx
  raise Exception(f"Instruction '{name} {arg}' not found")

def get_instr_name(code: int):
  assert code < 256
  instr = instructions[code]
  return f"{instr.name} {instr.arg}"

def get_instr(name: str, arg: str):
  return instructions[get_instr_code(name, arg)]

"""Core instructions; All possible ALU operations, with options for writeback"""

class CoreInstruction(Instruction):
    def __init__(self, alu_code, src, wb):
       self.alu_code = alu_code
       self.src = src
       self.wb = wb

    @property
    def name(self):
      instr_name = f"{self.alu_code}"
      if self.wb: instr_name += "WB"
      return instr_name
    
    @property
    def arg(self):
       return str(self.src)

    def run(self, flags):
      if self.src == Reg.IMM or self.src == Reg.REGS_OF_IMM:
        yield Step(Args.INC_PC)
        yield Step(Args.LD_IMM)

      # Perform operation
      yield Step(Args.fromRegs(self.src, Reg.W), self.alu_code)

      # Writeback (optional)
      if self.wb:
        yield Step(Args.fromRegs(Reg.W, self.src))
      
      # Increment program counter for next instruction
      yield Step(Args.INC_PC)
      yield ResetStep()

# Create base ALU instructions
for wb in [False, True]: #writeback
  for alu_code in AluCode:
    for src in Reg:
      # Skip invalid
      if src == Reg.W: continue
      if not src.value.can_src: continue
      if wb and not src.value.can_dst: continue
      if wb and alu_code == AluCode.WLOAD: continue
      if alu_code == AluCode.TODOINC: continue # TODO

      # Create instruction
      #print(src)
      instructions.append(CoreInstruction(alu_code, src, wb))


"""Store instructions; Write back accumulator W to any register"""
class StoreInstruction(Instruction):
    def __init__(self, dst):
       self.dst = dst

    @property
    def name(self): return "WSTORE"
    @property
    def arg(self): return str(self.dst)

    def run(self, flags):
      if self.dst == Reg.IMM or self.dst == Reg.REGS_OF_IMM:
        yield Step(Args.INC_PC)
        yield Step(Args.LD_IMM)

      # Writeback & incr PC
      yield Step(Args.fromRegs(Reg.W, self.dst))
      yield Step(Args.INC_PC)
      yield ResetStep()

# Create STORE instructions
for dst in Reg:
  if dst == Reg.W: continue
  if not dst.value.can_dst: continue
  instructions.append(StoreInstruction(dst))

"""Create (conditional) full JMP instruction"""

class CondJmpInstruction(Instruction):
    def __init__(self, name: str, condition_generator):
       self._name = name
       self.condition_generator = condition_generator

    @property
    def name(self): return self._name
    @property
    def arg(self): return "IMM16"

    def run(self, flags):
      yield Step(Args.INC_PC)
      yield Step(Args.LD_IMM)
      yield Step(Args.fromRegs(Reg.IMM, Reg.W), AluCode.WLOAD) # LD 1st part in W reg
      yield Step(Args.INC_PC)
      yield from self.condition_generator(flags,
        _if=[
            # JMP, perfor, remainder of JmpInstruction
            Step(Args.LD_IMM), # LD 2nd part in IMM reg
            Step(Args.fromRegs(Reg.W, Reg.PC_L)),
            Step(Args.fromRegs(Reg.IMM, Reg.W), AluCode.WLOAD),
            Step(Args.fromRegs(Reg.W, Reg.PC_H)),
        ],
        _else=[
            # No JMP, increment PC and continue
            Step(Args.INC_PC),
        ]
      )
      yield ResetStep()

instructions.append(CondJmpInstruction("JMP" , if_always))
instructions.append(CondJmpInstruction("JMPZ", if_zero_flag))
instructions.append(CondJmpInstruction("JMPC", if_carry_flag))


"""NOP instruction; Do nothing"""

assert len(instructions) <= 254 # Need to add two more

class NopInstruction(Instruction):
    @property
    def name(self): return "NOP"
    @property
    def arg(self): return ""

    def run(self, flags):
      yield Step(Args.INC_PC)
      yield ResetStep()

while len(instructions) < 255:
  instructions.append(NopInstruction())

"""HALT instruction; hangs the processor"""
instructions_count = len(instructions)

class HaltInstruction(Instruction):
    @property
    def name(self): return "HALT"
    @property
    def arg(self): return ""

    def run(self, flags):
      # Don't increment PC -> hang
      yield ResetStep()

instructions.append(HaltInstruction())







"""VERIFICATION STEP"""

# Make sure they all run without errors
for instr in instructions:
  for flags in range(4):
    # Run all steps ;)
    for step_idx, step in enumerate(instr.run(flags)):
      # First step should load W, to clear reset bit
      pass #if step_idx == 0: assert

# Make sure they are unique (except NOP)
for idx, instr in enumerate(instructions):
  if instr.name == "NOP": continue
  lookup_idx = get_instr_code(instr.name, instr.arg)
  if idx != lookup_idx:
    raise Exception(f"Duplicate instruction '{instr.name} {instr.arg}'")









"""LOG ALL INSTRUCTIONS"""
log = True
log_detail = False
count = 0
if log:
  for idx, instr in enumerate(instructions):
    # Skip duplicates
    if get_instr_code(instr.name, instr.arg) != idx:
      continue
    
    count += 1
    if log_detail:
      print(f"======= {idx:03d} =======")
      print(f"{instr.name} {instr.arg}")
      if type(instr) == CoreInstruction:
        if instr.alu_code == AluCode.WNEG:
          print(f"{Reg.W} = -{Reg.W}")
        else:
          print(f"{Reg.W} {instr.alu_code.symbol()}= {instr.src}")
      print(f"===================")
      print(("\x1B[3m{}  i:  ctrl     move       \x1B[0m" * 4).format(*["00", "01", "10", "11"]))
      table = instr.generate_table(pad=False)
      for step_idx in range(len(table[0])):

        print((" |  {:02d}: {:20}" * 4).format(
          step_idx, str(table[0][step_idx]), step_idx, str(table[1][step_idx]),
          step_idx, str(table[2][step_idx]), step_idx, str(table[3][step_idx])
          ))
      print()
    else:
      print(f"{idx:03d} | {instr.name:7} {instr.arg}")

  print(f"Used {100*count/256:.1f}% of instructions")
  