#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
sha256sum "$1" | grep "af8c4c0aa62adb1559c7809180f0d1624fb0094b00c4248b9a9c427555d10971"
if [ $? -ne 0 ]; then
  echo "Bad gc_devo_src.zip checksum, r266 only"
  exit 1
fi

unzip "$1"

mkdir work
pushd work
wget https://download.qemu.org/qemu-7.0.0.tar.xz
tar -xvf qemu-7.0.0.tar.xz
rm qemu-7.0.0.tar.xz
pushd qemu-7.0.0
patch -p1 < ../../qemu/add_broadway.patch
./configure --target-list=ppc-softmmu
make -j$(nproc)
pushd build
tail -c +353 ../../../loader.bin | head -c -8 > payload.bin
./qemu-system-ppc -display none -S -gdb tcp::20000,ipv4 -M broadway &
QEMU_PID=$!
sleep 1
gdb-multiarch -x ../../../scripts/devo_go_standalone.gdb
kill $QEMU_PID
cp devo_dump_0x80021470.bin ../../../
popd
popd
popd
rm -rf work

# Decrypt the DSP firmware cuz why not
tail -c +87457 devo_dump_0x80021470.bin | head -c 3072 > enc_dsp_payload.bin
python3 scripts/decrypt_dsp_payload.py

# Build the cracked loader.bin
cp patches/preloader.bin cracked_loader.bin
# Copy the name, rev, and date
dd if=devo_dump_0x80021470.bin skip=$((0x3394)) bs=1 count=$((0x48)) of=cracked_loader.bin conv=notrunc seek=4
# Copy a donor BCA needed
dd if=patches/donor.bca of=cracked_loader.bin conv=notrunc seek=$((0x240)) bs=1

# Apply the no-disc patches
xdelta3 -d -s devo_dump_0x80021470.bin patches/devo_patch.xdelta3 devo_dump_cracked.bin
dd if=devo_dump_cracked.bin of=cracked_loader.bin conv=notrunc seek=$((0x280)) bs=1

# Apply the keygen-only patch for Wii U
xdelta3 -d -s devo_dump_cracked.bin patches/keygen_only_patch.xdelta3 devo_dump_keygen.bin
dd if=devo_dump_keygen.bin of=keygen_loader.bin conv=notrunc seek=$((0x280)) bs=1

