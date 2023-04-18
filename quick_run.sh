#!/bin/bash

function run () {
	cmake --build build/ --target $1 --config Debug && build/bin/$1
}

function benchmark() {
	# only works for windows
	# set -e # -e crashes bash on windows for some reason if the command fails
	mkdir -p benchmark
	cd benchmark
	conan install -s build_type=Release ../..
	cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_SLEEP=1 ../..
	cmake --build . --target benchmark --config Release
	../../compile.sh
	bin/benchmark
	cd ..
}