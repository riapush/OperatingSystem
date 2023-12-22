#!/bin/bash

mkdir tmp; cd tmp

cmake -S ../ -B ./; make

mv set_tests ../

cd ../; rm -r tmp