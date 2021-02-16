CXXFLAGS=-ggdb -O0 -pedantic -Wall -fPIC
CC=g++

SRC=laas.cc ft3.cc isol.cc portmask.cc
HDR=laas.h ft3.h isol.h portmask.h

all: _laas.so

portmask: portmask.h portmask.cc

laas_wrap.cxx: laas.h ft3.h isol.h laas.i
	swig -c++ -python laas.i

_laas.so: laas_wrap.cxx $(SRC) $(HDR)
	python setup.py build_ext --inplace

clean:
	- \rm *.o _laas.so laas_wrap.cxx
	- \rm -rf build isol.log *.pyc logfile
