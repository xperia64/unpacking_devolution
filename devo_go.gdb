set architecture powerpc:750
set endian big
set pagination off
target extended-remote localhost:20000
source ../../../devo_unpack_gdb.py
quit
