#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
cp -R ./shared/work/ .
pushd ./work/gc_devo
pushd ./source
patch -p1 < ../../../main.patch
popd
mkdir -p apps/gc_devo
mkdir -p apps/gc_devo_keygen
cp icon.png apps/gc_devo
cp icon.png apps/gc_devo_keygen
cp meta.xml apps/gc_devo
cp meta.xml apps/gc_devo_keygen
sed -i 's/<name>Devolution<\/name>/<name>Devolution Cracked<\/name>/g' apps/gc_devo/meta.xml
sed -i 's/<name>Devolution<\/name>/<name>Devolution Keygen<\/name>/g' apps/gc_devo_keygen/meta.xml
cp ../cracked_loader.bin data/loader.bin
make
cp boot.dol apps/gc_devo/
cp ../cracked_loader.bin apps/gc_devo/loader.bin
cp ../keygen_loader.bin data/loader.bin
make
cp boot.dol apps/gc_devo_keygen/
popd
mkdir ./shared/output
cp -R ./work/gc_devo/apps ./shared/output
rm -r ./shared/work/
