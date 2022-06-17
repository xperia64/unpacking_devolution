#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
sha256sum loader.bin | grep "0c15f5f494f6586a9ca35dd928ca26e31a1c7510dfe34c14c645ed2f517f3f18"
if [ $? -ne 0 ]; then
  echo "Bad loader.bin checksum, r266 only"
  exit 1
fi
mkdir work
pushd work
wget https://download.qemu.org/qemu-7.0.0.tar.xz
tar -xvf qemu-7.0.0.tar.xz
rm qemu-7.0.0.tar.xz
pushd qemu-7.0.0
patch -p1 < ../../add_broadway.patch
./configure --target-list=ppc-softmmu
make -j8
pushd build
tail -c +353 ../../../loader.bin | head -c -8 > payload.bin
./qemu-system-ppc -display none -S -gdb tcp::20000,ipv4 -M broadway &
QEMU_PID=$!
sleep 1
gdb-multiarch -x ../../../devo_go.gdb
kill $QEMU_PID
cp devo_dump_0x80021470.bin ../../../
popd
popd
popd
rm -rf work
tail -c +87457 devo_dump_0x80021470.bin | head -c 3072 > enc_dsp_payload.bin
python3 decrypt_dsp_payload.py
