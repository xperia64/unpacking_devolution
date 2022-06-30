#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
docker build --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) -f Dockerfile.extract -t devo_extract .
docker build --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) -f Dockerfile.build -t devo_build .
docker run --rm -v $PWD/put_gc_devo_src_zip_here:/devo/shared devo_extract ./extract.sh
docker run --rm -v $PWD/put_gc_devo_src_zip_here:/devo/shared devo_build ./build_dol.sh
