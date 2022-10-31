CFLAGS=/nologo /MD /EHsc

all: build

build: so-cpp.exe

so-cpp.exe: main.obj hashmap.obj
	cl $(CFLAGS) /Feso-cpp.exe $**

main.obj: main.c
	cl $(CFLAGS) /c main.c
	
hashmap.obj: hashmap.c
	cl $(CFLAGS) /c hashmap.c

clean:
	del /Q /F *.obj so-cpp.exe
