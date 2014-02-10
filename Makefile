CXXFLAGS += -O2 -g -std=c++0x -I/opt/zmq3/include -I/opt/msgpack/include
LDFLAGS += -L/opt/zmq3/lib -L/opt/msgpack/lib
LDLIBS += -lueye_api -lzmq -lmsgpack

all: mosley

.PHONY: clean

clean:
	rm -f *.o
	find . -type f -perm +111 -print | xargs rm -f
