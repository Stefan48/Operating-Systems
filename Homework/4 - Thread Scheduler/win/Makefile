CFLAGS = /nologo /MD /EHsc /W3 /D_CRT_SECURE_NO_DEPRECATE

all: build

build: libscheduler.dll
	
libscheduler.dll: so_scheduler.obj
	link /nologo /dll /out:libscheduler.dll /implib:libscheduler.lib so_scheduler.obj
	
so_scheduler.obj: so_scheduler.c
	cl $(CFLAGS) /DDLL_EXPORTS /c so_scheduler.c
	
.PHONY: clean
clean:
	del /Q /F *.obj *.dll *.lib