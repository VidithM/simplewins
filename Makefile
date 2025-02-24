all:
	@gcc main.c -I/usr/include/libdrm -L/usr/lib/x86_64-linux-gnu -ldrm -o main
