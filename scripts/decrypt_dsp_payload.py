ctr = 0x300
r3 = -4
r5 = 0
out = [None] * 0x300

with open("enc_dsp_payload.bin", "rb") as f:
    b = f.read()
    while ctr != 0:
        r3 += 4
        r0 = b[r3 ^ 0b111]
        print(hex(r0))
        r7 = b[(r3 + 1) ^ 0b111]
        print(hex(r7))
        r8 = b[(r3 + 2) ^ 0b111]
        print(hex(r8))
        r7 = ((r0 << 8) & 0xFF00) + (r7 & 0xFFFF00FF)
        print(hex(r7))
        r0 = b[(r3 + 3) ^ 0b111]
        print(hex(r0))
        r7 = ((r8 << 24) & 0xFF000000) + (r7 & 0xFFFFFF)
        print(hex(r7))
        r7 = ((r7 << 17) | ((r7 >> (32 - 17)) & ~(-1 << 17))) & 0xFFFFFFFF
        print(hex(r7))
        r7 = ((r0 << 1) & 0x1FE) + (r7 & 0xFFFFFE01)
        print(hex(r7))
        r5 = r5 ^ r7
        print(hex(r5))
        out[(0x300 - ctr) ^ 0b1] = r5
        ctr -= 1

with open("dec_dsp_payload.bin", "wb") as f:
    for i in out:
        f.write(i.to_bytes(4, 'big'))
