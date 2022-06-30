# Paste this into Ghidra's python console with a range of addresses to dword swap
def munge():
    mem = currentProgram.getMemory()
    addr_iter = currentSelection.getAddresses(True)
    lo_addrs = [None] * 4
    hi_addrs = [None] * 4
    for addr in addr_iter:
        if None in lo_addrs:
            lo_addrs[addr.getOffset() & 3] = addr
        else:
            hi_addrs[addr.getOffset() & 3] = addr
            if not None in hi_addrs:
                lo_data = [mem.getByte(a) for a in lo_addrs]
                hi_data = [mem.getByte(a) for a in hi_addrs]
                for a, b in zip(hi_addrs, lo_data):
                    mem.setByte(a, b)
                for a, b in zip(lo_addrs, hi_data):
                    mem.setByte(a, b)
                lo_addrs = [None] * 4
                hi_addrs = [None] * 4
