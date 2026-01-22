all: build/stb_image.a

build/stb_image.a: build
	$(CC) -Iinclude/stb -c stb_image.c -o build/stb_image.o
	$(AR) rcs build/stb_image.a build/stb_image.o

build:
	mkdir build

clean:
	rm -rf build