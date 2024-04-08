from abc import ABC, abstractmethod
from enum import Enum
import enum
import re

"""Part of instruction: ALU operation to perform"""
class AluCode(Enum):
    def __str__(self):
        return self.name
    
    def symbol(self):
        if self == AluCode.WADD: return '+'
        elif self == AluCode.WSUB: return '-'
        elif self == AluCode.WAND: return '&'
        elif self == AluCode.WOR: return '|'
        elif self == AluCode.WXOR: return '^'
        elif self == AluCode.WLOAD: return ''
        raise TypeError(f"{self} doesn't have a symbol")
    
#   NAME    CODE
    WADD    = 0
    WSUB    = 1
    WINC    = 2 # TODO - wire in HW
    NOP     = 3 # TODO - wire in HW
    WAND    = 4
    WOR     = 5
    WXOR    = 6
    WLOAD   = 7

"""Registers of the CPU"""
class Reg(Enum):
    @enum.nonmember
    class RegsCapabilities:
      def __init__(self, src: bool, dst: bool):
        # capabilities to interact with bus
        self.can_src = src
        self.can_dst = dst
    
    def __str__(self):
        name = re.sub(r"_OF_(.*)", r"[\g<1>", self.name)
        name = re.sub(r"\[([^_]*)$", r"[\g<1>]", name)
        return re.sub(r"\[([^_]*)_", r"[\g<1>]_", name)
    
    W           = RegsCapabilities( True,  True)
    PC_L        = RegsCapabilities( True,  True)
    PC_H        = RegsCapabilities( True,  True)
    IMM         = RegsCapabilities( True, False)
    SP          = RegsCapabilities( True,  True)
    REGS_OF_IMM = RegsCapabilities( True,  True)
    REGS_OF_SP  = RegsCapabilities( True,  True)
    OUT_A       = RegsCapabilities(False,  True)
    OUT_B       = RegsCapabilities(False,  True)

"""Part of instruction: operand to select"""
class Args(Enum):
    @enum.nonmember
    class ArgsInner:
        def __init__(self, code: int, src: Reg, dst: Reg):
          if src and not src.value.can_src:
            raise ValueError(f"Register {src} can not be used as src")
          if dst and not dst.value.can_dst:
            raise ValueError(f"Register {dst} can not be used as dst")

          self.code = code
          self.src = src
          self.dst = dst
        
        def __str__(self):
          return f"{self.src}->{self.dst}"
    
    @classmethod
    def fromRegs(cls, src: Reg, dst: Reg):
        if not src or not dst: raise ValueError(f"Illegal move")
        for args in Args:
          op_src = args.value.src
          op_dst = args.value.dst
          if src == op_src and dst == op_dst:
              return args
        raise ValueError(f"Illegal move: {src.name} -> {dst.name}")

    def __str__(self):
       if not self.value.src or not self.value.dst:
          return self.name
       return str(self.value)
    
    # accumulator OUT
    LD_INSTR         = ArgsInner(0, None, None)
    INC_PC           = ArgsInner(1, None, None)

    # accumulator IN
    PCL_TO_W         = ArgsInner(2, Reg.PC_L, Reg.W)
    PCH_TO_W         = ArgsInner(3, Reg.PC_H, Reg.W)

    # accumulator IN
    IMM_TO_W         = ArgsInner(4, Reg.IMM, Reg.W)
    SP_TO_W          = ArgsInner(5, Reg.SP, Reg.W)
    REGS_OF_IMM_TO_W = ArgsInner(6, Reg.REGS_OF_IMM, Reg.W)
    REGS_OF_SP_TO_W  = ArgsInner(7, Reg.REGS_OF_SP, Reg.W)

    # accumulator OUT
    W_TO_OUTA        = ArgsInner(8, Reg.W, Reg.OUT_A)
    W_TO_OUTB        = ArgsInner(9, Reg.W, Reg.OUT_B)
    W_TO_PCL         = ArgsInner(10, Reg.W, Reg.PC_L)
    W_TO_PCH         = ArgsInner(11, Reg.W, Reg.PC_H)

    # accumulator OUT
    LD_IMM           = ArgsInner(12, None, None)
    W_TO_SP          = ArgsInner(13, Reg.W, Reg.SP)
    W_TO_REGS_OF_IMM = ArgsInner(14, Reg.W, Reg.REGS_OF_IMM)
    W_TO_REGS_OF_SP  = ArgsInner(15, Reg.W, Reg.REGS_OF_SP)

"""Single µstep of an instruction"""
class InstructionStep:
  def __str__(self):
     if self.rst:
        return "RESET"
     
     op_str = str(self.alu_op) if self.alu_op else ""
     return f"{op_str:8} {self.args}"

  def __init__(self, args:Args = None, alu_op:AluCode = None, rst:bool = False):
    if (args and args.value.dst == Reg.W):
       assert alu_op # If storing to W alu_op should be defined
    self.args = args
    self.alu_op = alu_op
    self.rst = rst

  def code(self):
    args = self.args or Args.LD_INSTR
    alu_op = self.alu_op or AluCode.WADD
    rst = self.rst

"""Full instruction"""
class Instruction(ABC):
    @property
    @abstractmethod
    def name(self):
       raise NotImplementedError("Instruction not implemented correctly")
    
    @property
    @abstractmethod
    def arg(self):
       raise NotImplementedError("Instruction not implemented correctly")

    @abstractmethod  # Decorator to define an abstract method
    def run(self, flags):
       raise NotImplementedError("Instruction not implemented correctly")


"""
Helper functions for defining instructions
=> to make sure all conditional branches are balanced
"""

def balanced_yield(idx, lists):
    # Yield selected list items
    for x in lists[idx]:
        yield x
        
    # pad to max branch length
    max_len = max(len(x) for x in lists )
    nops = max_len - len(lists[idx])
    for x in range(nops):
        yield InstructionStep(alu_op=AluCode.NOP)
    
def if_flag(flags, _if00=None, _if01=None, _if10=None, _if11=None, _else=InstructionStep(alu_op=AluCode.NOP)):
    options = [_if00, _if01, _if10, _if11]
    for i in range(4):
        if not options[i]:
            options[i] = _else
    yield from balanced_yield(flags, options)
    
def if_zero_flag(flags, _if, _else):
    yield from if_flag(flags, _if01=_if, _if11=_if, _else=_else)
    
def if_carry_flag(flags, _if, _else):
    yield from if_flag(flags, _if10=_if, _if11=_if, _else=_else)
    
def if_both_flag(flags, _if, _else):
    yield from if_flag(flags, _if11=_if, _else=_else)

def if_always(flags, _if, _else):
    yield from if_flag(flags, _else=_if)

def if_never(flags, _if, _else):
    yield from if_flag(flags, _else=_else)
