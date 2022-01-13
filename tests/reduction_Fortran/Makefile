red_Fortran = testF
red_Fortran_OBJ = testF_c.o testF_f.o

.PHONY: all

all: $(red_Fortran)

$(red_Fortran): $(red_Fortran_OBJ)
	$(CXX) testF_f.o testF_c.o ../../lib/IcarUPCXX.a $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o testF

testF_f.o: testF.f90
	gfortran -c testF.f90 -o testF_f.o

testF_c.o: testF.cpp 
	$(CXX) -c testF.cpp -o testF_c.o 

.PHONY: clean

clean:
	        $(RM) $(red_Fortran_OBJ) $(red_Fortran)
