import sys
from Crypto.Cipher import AES
dvv = open(sys.argv[1], "rb")
d = dvv.read()
dvv.close()
iv = d[:12]
iv = iv+d[0x1c:0x20]
encrypted = d[0x20:]
# Put devo key from main PPC payload @ 0xb3215560 here
key = b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
cipher = AES.new(key, AES.MODE_CBC, iv=iv)
decrypted = cipher.decrypt(encrypted)
decf = open(sys.argv[2], "wb")
decf.write(decrypted)
decf.close()
