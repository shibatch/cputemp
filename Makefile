all : cputemp

cputemp : cputemp.c
	$(CC) -O cputemp.c -o cputemp

clean :
	rm -f cputemp *~
