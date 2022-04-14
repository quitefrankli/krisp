function run () {
	cmake --build . --target $1 --config Debug && bin/$1
}