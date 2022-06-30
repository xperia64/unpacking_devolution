#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
sha256sum ./shared/gc_devo_src.zip | grep "af8c4c0aa62adb1559c7809180f0d1624fb0094b00c4248b9a9c427555d10971"
if [ $? -ne 0 ]; then
  echo "Bad gc_devo_src.zip checksum, r266 only"
  exit 1
fi

unzip ./shared/gc_devo_src.zip

tail -c +353 ./gc_devo/data/loader.bin | head -c -8 > ./payload.bin
qemu-system-ppc -display none -S -gdb tcp::20000,ipv4 -M broadway &
QEMU_PID=$!
sleep 5
gdb-multiarch -x ./scripts/devo_go.gdb
kill $QEMU_PID

# Decrypt the DSP firmware cuz why not
tail -c +87457 devo_dump_0x80021470.bin | head -c 3072 > enc_dsp_payload.bin
python3 ./scripts/decrypt_dsp_payload.py

# Build the cracked loader.bin
cp ./patches/preloader.bin ./cracked_loader.bin
# Copy the name, rev, and date
dd if=./devo_dump_0x80021470.bin skip=$((0x3394)) bs=1 count=$((0x48)) of=./cracked_loader.bin conv=notrunc seek=4
# Copy a donor BCA needed
dd if=./patches/donor.bca of=./cracked_loader.bin conv=notrunc seek=$((0x240)) bs=1

# Apply the no-disc patches
xdelta3 -d -s devo_dump_0x80021470.bin patches/devo_patch.xdelta3 devo_dump_cracked.bin
dd if=devo_dump_cracked.bin of=cracked_loader.bin conv=notrunc seek=$((0x280)) bs=1

# Apply the keygen-only patch for Wii U
cp cracked_loader.bin keygen_loader.bin
xdelta3 -d -s devo_dump_cracked.bin patches/keygen_only_patch.xdelta3 devo_dump_keygen.bin
dd if=devo_dump_keygen.bin of=keygen_loader.bin conv=notrunc seek=$((0x280)) bs=1

# Ship it
mkdir -p ./shared/work
cp -R ./gc_devo ./shared/work
cp cracked_loader.bin ./shared/work
cp keygen_loader.bin ./shared/work

mkdir -p ./shared/for_devs
cp devo_dump_0x80021470.bin ./shared/for_devs
cp dec_dsp_payload.bin ./shared/for_devs
