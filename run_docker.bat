set batch_path=%~dp0%
cd "%batch_path%"
set shared_path=%batch_path%put_gc_devo_src_zip_here
set shared_path=%shared_path:\=/%
echo "%shared_path%"
docker build --build-arg USER_ID=1000 --build-arg GROUP_ID=1000 -f Dockerfile.extract -t devo_extract .
docker build --build-arg USER_ID=1000 --build-arg GROUP_ID=1000 -f Dockerfile.build -t devo_build .
docker run --rm -v "%shared_path%":/devo/shared devo_extract ./extract.sh
docker run --rm -v "%shared_path%":/devo/shared devo_build ./build_dol.sh
echo "Complete!"
pause