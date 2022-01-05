OBJECTS = RTS_impl.o

IcarUPCXX.a: $(OBJECTS)
	ar $(ARFLAGS) $@ $(OBJECTS)
	mv IcarUPCXX.a ../lib

$(OBJECTS): 

RTS_impl.o: RTS_impl.cpp

.PHONY: clean

clean:
	$(RM) $(OBJECTS)
