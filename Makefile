CXXFLAGS += -O2 -g -std=c++0x
LDLIBS += -lueye_api -lopencv_core -lopencv_highgui

all: mosley

.PHONY: clean

clean:
	rm -f *.o
	find . -type f -perm +111 -print | xargs rm -f
