all : cputemp

cputemp : cputemp.cpp
	$(CXX) -Os cputemp.cpp -o cputemp -lbsd -lm

clean :
	rm -f cputemp *~
