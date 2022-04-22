#!/bin/bash

function run () {
	cmake --build . --target $1 --config Debug && bin/$1
}

function benchmark() {
	# only works for windows
	cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_SLEEP=1 .. -A x64
	cmake --build . --target benchmark --config Release
	../compile.sh
	bin/benchmark
}