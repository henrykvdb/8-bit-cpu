from abc import ABC, abstractmethod
from enum import Enum
import enum
import re

"""Part of instruction: ALU operation to perform"""
class AluCode(Enum):
    def __str__(self):
        return self.name

    def code(self):
       return self.value
    
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
    WNEG    = 2
    NOP     = 3
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
    TMP         = RegsCapabilities( True,  True)
    PC_L        = RegsCapabilities( True,  True)
    PC_H        = RegsCapabilities( True,  True)
    SP          = RegsCapabilities( True,  True)
    REGS_OF_IMM = RegsCapabilities( True,  True)
    REGS_OF_SP  = RegsCapabilities( True,  True)
    IMM         = RegsCapabilities( True, False)
    IN          = RegsCapabilities( True, False)
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

    def code(self):
       return self.value.code
    
    # ACC_IN => lowest bit ArgsInner.code

    
    ### 0-7 (above PC-L)

    INC_PC           = ArgsInner(0, None, None)       # IN (rst delay reg on W_IN)

    W_TO_OUTB        = ArgsInner(1, Reg.W, Reg.OUT_B) # OUT TODO HW (add reg)

    TMP_TO_W         = ArgsInner(2, Reg.TMP, Reg.W) # IN TODO
    W_TO_TMP         = ArgsInner(3, Reg.W, Reg.TMP) # OUT TODO

    PCL_TO_W         = ArgsInner(4, Reg.PC_L, Reg.W) # IN
    W_TO_PCL         = ArgsInner(5, Reg.W, Reg.PC_L) # OUT

    PCH_TO_W         = ArgsInner(6, Reg.PC_H, Reg.W) # IN
    W_TO_PCH         = ArgsInner(7, Reg.W, Reg.PC_H) # OUT

    ### 8-15 (above ram muxing)

    SP_TO_W          = ArgsInner(8 + 0, Reg.SP, Reg.W) # IN
    W_TO_SP          = ArgsInner(8 + 1, Reg.W, Reg.SP) # OUT

    REGS_OF_SP_TO_W  = ArgsInner(8 + 2, Reg.REGS_OF_SP, Reg.W)  # IN
    W_TO_REGS_OF_SP  = ArgsInner(8 + 3, Reg.W,  Reg.REGS_OF_SP) # OUT

    IMM_TO_W         = ArgsInner(8 + 4, Reg.IMM, Reg.W) # IN
    LD_IMM           = ArgsInner(8 + 5, None, None)     # X

    REGS_OF_IMM_TO_W = ArgsInner(8 + 6, Reg.REGS_OF_IMM, Reg.W) # IN
    W_TO_REGS_OF_IMM = ArgsInner(8 + 7, Reg.W, Reg.REGS_OF_IMM) # OUT

    ### 16-23 (not decoded)

    ### 24-31 (above PC-H)

    IN_TO_W          = ArgsInner(2, Reg.IN, Reg.W)    # IN TODO HW (add reg) 24
    W_TO_OUTA        = ArgsInner(3, Reg.W, Reg.OUT_A) # OUT TODO HW (wire reg) 25

"""Single µstep of an instruction"""
class Step:
  def __str__(self):
    if self.rst:
       return "RESET"
    if self.alu_op == AluCode.NOP and self.args == Args.PCL_TO_W:
       return "NOP"
    op_str = str(self.alu_op) if self.alu_op else ""
    args_string = str(self.args) if self.args else ""
    return f"{op_str:8} {args_string}"

  def __init__(self, args:Args = None, alu_op:AluCode = None, rst:bool = False):
    set_defaults = False

    # Edge case: reset
    if rst:
       assert not (args or alu_op)
       set_defaults = True
    
    # Edge case: NOP
    if alu_op == AluCode.NOP:
       assert not args
       set_defaults = True
    
    # Edge case: INC_PC
    if args in [Args.INC_PC, Args.LD_IMM]:
       assert not alu_op
       set_defaults = True
    
    # Set defaults / Do sanity checks
    if set_defaults:
       alu_op = alu_op or AluCode.NOP
       args = args or Args.PCL_TO_W
    else:
       assert args is not None
       if (args.value.dst == Reg.W):
          assert alu_op is not None
       else:
          alu_op = alu_op or AluCode.NOP
          assert alu_op == AluCode.NOP

    self.args = args
    self.alu_op = alu_op
    self.rst = rst

  def code(self):
    value = 0
    value += (0 if self.rst else 1) << 7
    value +=     self.alu_op.code() << 4
    value +=       self.args.code() << 0
    return value

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
    
    def generate_table(self, pad=True):
        # Generate table
        table = []
        for flags in range(4):
          steps = list(self.run(flags))
          assert len(steps) <= 16
          table.append(steps)
        
        # All branches should be equal length
        assert len({len(i) for i in table}) == 1

        # Pad with NOP
        if pad:
          for flags in range(4):
            for _ in range(16 - len(table[flags])):
              table[flags].append(Step(rst=True))
        
        return table


"""
Helper functions for defining instructions
=> to make sure all conditional branches are balanced
"""

def balanced_yield(branch_idx, branches):
    # Yield selected list items
    for x in branches[branch_idx]:
        yield x
        
    # pad to max branch length
    max_len = max(len(x) for x in branches )
    nops = max_len - len(branches[branch_idx])
    for x in range(nops):
        yield Step(alu_op=AluCode.NOP)
    
def if_flag(flags, _if00=None, _if01=None, _if10=None, _if11=None, _else=Step(alu_op=AluCode.NOP)):
    branches = [_if00, _if01, _if10, _if11]
    for i in range(4):
        if branches[i] == None:
            branches[i] = _else
    yield from balanced_yield(flags, branches)
    
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
