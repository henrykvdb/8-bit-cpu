from enum import Enum
import enum
import re
from instruction_types import *

instructions = []

from dataclasses import dataclass
from typing import Callable

def balanced_yield(idx, lists):
    # Yield selected list items
    for x in lists[idx]:
        yield x
        
    # pad to max branch length
    max_len = max(len(x) for x in lists )
    nops = max_len - len(lists[idx])
    for x in range(nops):
        yield -1
    
def if_zero(flags, _if, _else):
    idx = 0 if (flags & 1 == 1) else 1
    yield from balanced_yield(idx, [_if, _else])

def some_instruction(flags):
    yield 0
    yield 1
    yield from if_zero(flags, 
      _if=[2, 3],
      _else=[12, 13, 14]
    )
    yield 4
    yield 5

print(list(some_instruction(1)))
    

def run_instruction(flags: int, step: Callable[[InstructionStep], None]):
    step(InstructionStep(Args.LD_INSTR))
    step(InstructionStep(Args.fromRegs(Reg.REGS_OF_SP, Reg.W), AluCode.WINC))
    if_zero(flags=flags,
        _if=[step(InstructionStep(Args.fromRegs(Reg.W, Reg.REGS_OF_SP)))],
        _else=[step(InstructionStep(alu_op=AluCode.NOP))]
    )
    #if flags & 1:
    #  step(InstructionStep(Args.fromRegs(Reg.W, Reg.REGS_OF_SP)))
    step(InstructionStep(Args.INC_PC))
    step(InstructionStep(rst=True))


def generate_table():
    big_table = []

    step_bits = 4
    flag_bits = 2
    big_table.append([])

    for flags in range(2 ** flag_bits):
        steps = []

        def step(s: InstructionStep):
            steps.append(s)
            assert len(steps) < 2 ** step_bits

        run_instruction(flags, step)
        #while len(steps) < 2 ** step_bits:
        #    steps.append(InstructionStep("NOP"))

        big_table[-1].append(steps)

    return big_table


def main():
    table = generate_table()

    #for line in table:
    #    print("------")
    #    for subline in line:
    #        print(subline)


if __name__ == '__main__':
    main()
