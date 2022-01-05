hello = helloWorld
hello_OBJ = $(hello).o
hello_SRC = $(hello).cpp

.PHONY: all

all: $(hello)

$(hello): $(hello_OBJ) ../lib/IcarUPCXX.a
	$(CXX) $(hello_OBJ) ../lib/IcarUPCXX.a $(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) -o $(hello)

helloWorld.o: $(hello_SRC) 
	$(CXX) -c $(hello_SRC)  -o helloWorld.o

.PHONY: clean

clean:
	        $(RM) $(hello_OBJ) $(hello)
