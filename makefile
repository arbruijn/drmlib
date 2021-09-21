all: drmtest

drmtest: drmlib.c drmtest.c
	gcc -o $@ drmlib.c drmtest.c -I/usr/include/libdrm -ldrm -lgbm -lEGL -lGL
