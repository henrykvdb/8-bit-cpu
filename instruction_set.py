from enum import Enum
import enum
import re
from instruction_types import *

instructions = []

# Create base ALU instructions
for wb in [False, True]: #writeback
  for alu_code in AluCode:
    for src in Reg:
      # Skip invalid
      if src == Reg.W: continue
      if not src.value.can_src: continue
      if wb and not src.value.can_dst: continue
      if wb and alu_code == AluCode.WLOAD: continue
      if alu_code == AluCode.WINC:
         if src == Reg.PC_L or src == Reg.PC_H: continue

      # Prepare operand
      steps = [InstructionStep(Args.LD_INSTR)]
      if src == Reg.IMM or src == Reg.REGS_OF_IMM:
        steps.append(InstructionStep(Args.INC_PC))
        steps.append(InstructionStep(Args.LD_IMM))

      # Perform operation
      steps.append(InstructionStep(Args.fromRegs(src, Reg.W), alu_code))

      # Writeback (optional)
      if wb:
        steps.append(InstructionStep(Args.fromRegs(Reg.W, src)))
      
      # Increment program counter for next instruction
      steps.append(InstructionStep(Args.INC_PC))
      steps.append(InstructionStep(rst=True))

      # Create instruction
      instr_name = f"{alu_code}"
      if wb: instr_name += "WB"
      instructions.append(Instruction(instr_name, steps, f"{src}"))

# Create STORE instructions
for dst in Reg:
  # Skip invalid
  if dst == Reg.W: continue
  if not dst.value.can_dst: continue

  # Prepare operand
  steps = [InstructionStep(Args.LD_INSTR)]
  if dst == Reg.IMM or dst == Reg.REGS_OF_IMM:
    steps.append(InstructionStep(Args.INC_PC))
    steps.append(InstructionStep(Args.LD_IMM))

  # Writeback & incr PC
  steps.append(InstructionStep(Args.fromRegs(Reg.W, dst)))
  steps.append(InstructionStep(Args.INC_PC))
  steps.append(InstructionStep(rst=True))

  # Create instruction
  instructions.append(Instruction(f"WSTORE", steps, f"{dst}"))

# Create full jump instruction
instructions.append(Instruction(name="JMP", steps=[
    InstructionStep(Args.LD_INSTR),
    InstructionStep(Args.INC_PC),
    InstructionStep(Args.LD_IMM),
    InstructionStep(Args.fromRegs(Reg.IMM, Reg.W), AluCode.WLOAD),
    InstructionStep(Args.INC_PC),
    InstructionStep(Args.LD_IMM), # first part in W, second part in IMM
    InstructionStep(Args.fromRegs(Reg.W, Reg.PC_L)),
    InstructionStep(Args.fromRegs(Reg.IMM, Reg.W), AluCode.WLOAD),
    InstructionStep(Args.fromRegs(Reg.W, Reg.PC_H)),
    InstructionStep(rst=True),
], arg="IMM16"))
  


detail = False
for idx, instr in enumerate(instructions):
  if detail:
    print(f"======= {idx:03d} =======")
    print(f"{instr.name} {instr.arg}")
    print(f"{Reg.W} {alu_code.symbol()}= {src}")
    print(f"===================")
    print(f"\x1B[3mi: {"ctrl":8} move\x1B[0m")
    for step_idx, step in enumerate(instr.steps):
      print(f"{step_idx}: {step}")
    print()
  else:
    print(f"{idx:03d} | {instr.name:7} {instr.arg}")



print(f"Used {100*len(instructions)/256:.1f}% of instructions")
#print(Args.W_TO_REGS_OF_IMM.value)


#print(Args.W_TO_REGS_OF_IMM.name)