CXXFLAGS += -O2 -g -std=c++0x -I/opt/zmq3/include
LDFLAGS += -L/opt/zmq3/lib
LDLIBS += -lueye_api -lopencv_core -lopencv_highgui -lzmq

all: mosley

.PHONY: clean

clean:
	rm -f *.o
	find . -type f -perm +111 -print | xargs rm -f
