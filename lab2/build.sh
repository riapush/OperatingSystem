mkdir build
cd build
cmake -S ../ -B ./
make

rm -rf host*autogen
mv host* ../
cd ../

rm -r build
