all:
	gcc -Ofast bbc.c -o ../bin/bcc
	x86_x64-w64-mingw32-gcc -Ofast bbc.c -o ../bin/bbc.exe
debug:
	gcc -g bbc.c -o ../bin/bbc
	x86_x64-mingw32-gcc bbc.c -o ../bin/bbc.exe