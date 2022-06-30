#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
wget -q https://download.qemu.org/qemu-7.0.0.tar.xz
tar -xf qemu-7.0.0.tar.xz
rm qemu-7.0.0.tar.xz
pushd qemu-7.0.0
patch -p1 < ../add_broadway.patch
./configure --target-list=ppc-softmmu
make -j$(nproc)
make install
