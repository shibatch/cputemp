all : cputemp

cputemp : cputemp.c
	$(CC) -O cputemp.c -o cputemp -lm

clean :
	rm -f cputemp *~
