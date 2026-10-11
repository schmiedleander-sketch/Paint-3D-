# Makefile for building Paint 3D for Windows

CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
WIXL = wixl

CFLAGS = -O2 -Wall -mwindows
LDFLAGS = -static -static-libgcc -Wl,-Bstatic -lssp -Wl,-Bdynamic -Wl,--nxcompat -Wl,--dynamicbase -Wl,--high-entropy-va -lgdi32 -lcomctl32 -lcomdlg32

all: Paint3D.exe Paint3D.msi

app.ico: generate_icon.py
	python3 generate_icon.py

resource.o: resource.rc app.ico Paint3D.manifest
	$(WINDRES) resource.rc -O coff -o resource.o

Paint3D.exe: main.c resource.o
	$(CC) $(CFLAGS) main.c resource.o -o Paint3D.exe $(LDFLAGS)

Paint3D.msi: Paint3D.wxs Paint3D.exe app.ico
	$(WIXL) -v Paint3D.wxs -o Paint3D.msi

clean:
	rm -f Paint3D.exe Paint3D.msi resource.o app.ico app.png

.PHONY: all clean
