import gdb

def SetBp(ea):
    gdb.Breakpoint("*0x{:X}".format(ea))

def Go():
    gdb.execute("continue");

def SetPC(ea):
    gdb.execute("set $pc = 0x{:X}".format(ea))

def GetRegValue(reg):
    return gdb.selected_frame().read_register(reg)

def DMA(dmau, dmal):
    DMA_T = (dmal >> 1) & 1
    if (DMA_T):
        MEM_ADDR = (dmau >> 5) << 5
        LC_ADDR = (dmal >> 5) << 5

        MEM_ADDR |= 0x80000000

        DMA_LEN_U = (dmau & 0x1F) << 8
        DMA_LEN_L = (dmal >> 2) & 3

        LEN = DMA_LEN_U | DMA_LEN_L

        if (LEN == 0):
            LEN = 0x80

        DMA_LD = (dmal >> 4) & 1

        print("DMA: mem = 0x%X, cache = 0x%X, len = 0x%X, LD = %d\n" % (MEM_ADDR, LC_ADDR, LEN, DMA_LD))

        if (DMA_LD):
            buf = gdb.inferiors()[0].read_memory(MEM_ADDR, LEN)
            for i in range(len(buf)):
                gdb.inferiors()[0].write_memory(LC_ADDR+i, buf[i])
        else:
            buf = gdb.inferiors()[0].read_memory(LC_ADDR, LEN)
            for i in range(len(buf)):
                gdb.inferiors()[0].write_memory(MEM_ADDR+i, buf[i])

# SetBp(0x80004198)
SetBp(0x80004230)
Go()

# SetPC(0x800041D8)
SetPC(0x8000423C)

# DMAU = 0x8000A4A4
DMAU = 0x80025B14
SetBp(DMAU)
# DMAL = 0x8000A4C0
DMAL = 0x80025B30
SetBp(DMAL)

dmau = 0
dmal = 0

# CRC = 0x80006358

# New
#CRCS = [0x80020DB0, 0x800210D8, 0x8002118C, 0x80021470, 0x80021568, 0x80021720, 0x80021890]
CRCS = [0x80021470]

for CRC in CRCS:
    SetBp(CRC)

# BCTRL = 0x8000A984
BCTRL = 0x800260F4 # Probably this
SetBp(BCTRL)

files = [open("devo_dump_0x{:X}.bin".format(x), "wb") for x in CRCS]

while(True):
    Go()
    PC = GetRegValue("pc")
    if (PC == DMAU):
        dmau = GetRegValue("r0")
    elif (PC == DMAL):
        dmal = GetRegValue("r0")
        DMA(dmau, dmal)
    elif (PC in CRCS):
        idx = 999999
        for i in range(0, len(CRCS)):
            if PC == int(CRCS[i]):
                idx = i
                break
        R3 = GetRegValue("r3")
        R4 = GetRegValue("r4")
        print("DUMP: 0x%X 0x%X" % (R3, R4))
        data = gdb.inferiors()[0].read_memory(R3, R4).tobytes()
        files[idx].write(data)
    else:
        break

for f in files:
    f.close();
