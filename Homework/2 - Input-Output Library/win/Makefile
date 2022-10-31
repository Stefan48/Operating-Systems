CFLAGS = /nologo /MD /EHsc /W3 /D_CRT_SECURE_NO_DEPRECATE

all: build

build: so_stdio.dll
	
so_stdio.dll: so_stdio.obj
	link /nologo /dll /out:so_stdio.dll /implib:so_stdio.lib so_stdio.obj
	
so_stdio.obj: so_stdio.c
	cl $(CFLAGS) /DDLL_EXPORTS /c so_stdio.c
	
.PHONY: clean
clean:
	del /Q /F *.obj *.dll *.lib
