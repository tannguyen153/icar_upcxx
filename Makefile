######################################################################
INCLUDES = -I./include -I../src
LIBDIR = ./lib
export LIBGEN = $(LIBDIR)/IcarUPCXX.a
#FLAGS+= -DUSE_MPI_LAUNCHER

UPCXX_INSTALL=/opt/upcxx-2021.9.0
PPFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta PPFLAGS)
LDFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LDFLAGS)
LIBFLAGS=$(shell $(UPCXX_INSTALL)/bin/upcxx-meta LIBFLAGS)

export CXX = $(UPCXX_INSTALL)/bin/upcxx -network=mpi -O3 -DPRINT_OUT_EVENTS#$(PPFLAGS) $(LDFLAGS) $(LIBFLAGS) 
export LDLIBS += -L$(LIBGEN)
export CXXFLAGS += $(INCLUDES) $(FLAGS) $(ARCH_FLAGS) -O3
export LDFLAGS += $(FLAGS) $(ARCH_FLAGS)

######################################################################
SUBDIRS = src tests tests/reduction_Fortran
#export VPATH = ../include:../include/tests:../include/visitors \
               ../include/maps:../include/system:../include/dsl

######################################################################
.PHONY: build $(SUBDIRS)

build: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: all

all: build

######################################################################
.PHONY: clean

clean:
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) clean;) \
        $(RM) lib/libIcarUPCXX.a
