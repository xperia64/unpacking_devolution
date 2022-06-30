# Paste this into Ghidra's python console at a string
from ghidra.program.model.data import ArrayDataType
from ghidra.program.model.data import CharDataType

def get_string(addr):
    """Get strings one byte at a time"""
    output = b''
    localAddr = addr.add(0)
    while True:
        output += chr((getByte(localAddr) ^ int(str(localAddr), 16)) & 0xFF)
        localAddr = localAddr.add(1)
        if output[-1] == '\x00':
            break
    return output

def deobf():
    real_string = get_string(currentAddress)
    listing = currentProgram.getListing()
    codeUnit = listing.getCodeUnitAt(currentAddress)
    codeUnit.setComment(codeUnit.PRE_COMMENT, '"' + real_string[:-1].replace('\n', '\\n') + '"')
    createLabel(currentAddress, "s_%s" % (real_string[:-1].replace('\n','').replace(' ', '')), True)
    createData(currentAddress, ArrayDataType(CharDataType.dataType, len(real_string), 1))
    setCurrentLocation(currentAddress.add(len(real_string)))
