hello = helloWorld
hello_OBJ = $(hello).o
hello_SRC = $(hello).cpp

1DPropagation = 1DPropagation
1DPropagation_OBJ = $(1DPropagation).o
1DPropagation_SRC = $(1DPropagation).cpp


.PHONY: all

all: $(hello) $(1DPropagation)

$(hello): $(hello_OBJ) ../lib/IcarUPCXX.a
	$(CXX) $(hello_OBJ) ../lib/IcarUPCXX.a $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $(hello)

helloWorld.o: $(hello_SRC) 
	$(CXX) -c $(hello_SRC)  -o helloWorld.o

$(1DPropagation): $(1DPropagation_OBJ) ../lib/IcarUPCXX.a
	$(CXX) $(1DPropagation_OBJ) ../lib/IcarUPCXX.a $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $(1DPropagation)

1DPropagation.o: $(1DPropagation_SRC) 
	$(CXX) -c $(1DPropagation_SRC)  -o 1DPropagation.o

.PHONY: clean

clean:
	        $(RM) $(hello_OBJ) $(hello)
	        $(RM) $(1DPropagation_OBJ) $(1DPropagation)
