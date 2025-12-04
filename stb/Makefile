all: build/stb_image.a

build/stb_image.a: build
	gcc -Iinclude/stb -c stb_image.c -o build/stb_image.o
	ar rcs build/stb_image.a build/stb_image.o

build:
	mkdir build

clean:
	rm -rf build