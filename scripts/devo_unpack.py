import idc
import idaapi

def SetBp(ea):
	AddBptEx(ea, 1, BPT_EXEC)

def Wait():
	GetDebuggerEvent(WFNE_SUSP, -1)

def Go():
	ResumeProcess()
	Wait()

def SetPC(ea):
	SetRegValue(ea, "PC")

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

		print "DMA: mem = 0x%X, cache = 0x%X, len = 0x%X, LD = %d\n" % (MEM_ADDR, LC_ADDR, LEN, DMA_LD)

		if (DMA_LD):

			buf = idaapi.dbg_read_memory(MEM_ADDR, LEN)

			for i in range(len(buf)):
				idaapi.dbg_write_memory(LC_ADDR+i, buf[i])
		else:

			buf = idaapi.dbg_read_memory(LC_ADDR, LEN)

			for i in range(len(buf)):
				idaapi.dbg_write_memory(MEM_ADDR+i, buf[i])

SetBp(0x80004198)
Go()

SetPC(0x800041D8)

DMAU = 0x8000A4A4
SetBp(DMAU)
DMAL = 0x8000A4C0
SetBp(DMAL)

dmau = 0
dmal = 0

CRC = 0x80006358
SetBp(CRC)

BCTRL = 0x8000A984
SetBp(BCTRL)

f = open("devo_dump.bin", "wb")

while(True):

	Go()

	PC = GetRegValue("PC")

	if (PC == DMAU):

		dmau = GetRegValue("R0")

	elif (PC == DMAL):

		dmal = GetRegValue("R0")

		DMA(dmau, dmal)

	elif (PC == CRC):

		R3 = GetRegValue("R3")
		R4 = GetRegValue("R4")

		print "DUMP: 0x%X 0x%X" % (R3, R4)

		data = idaapi.dbg_read_memory(R3, R4)
		f.write(data)

	else:
			
		break

f.close()