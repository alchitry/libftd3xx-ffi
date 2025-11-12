# Define the dynamic library and its dependencies
LIBS = -L. -lftd3xx 
ifeq ($(shell uname -s),Linux)
    LIBS += -pthread -lrt
endif

# List of all dynamic demo executables
DEMOS = streamer asyncstreamer FtPthread async-loopback FTD3xx_Notification AbortRec rw

# Common compiler flags
COMMON_CFLAGS = -g -O3 -Wall -Wextra -ffunction-sections -fmerge-all-constants
CFLAGS = -std=c99 $(COMMON_CFLAGS) -D_POSIX_C_SOURCE
CXXFLAGS = -std=c++11 $(COMMON_CFLAGS)

# All rule to build all dynamic demos
.PHONY: all clean
all: $(addsuffix -dynamic,$(DEMOS))

# A pattern rule to build any dynamic demo
%-dynamic: %.o
	$(CC) $(COMMON_CFLAGS) -o $@ $^ $(LIBS)

# Specific rules for multi-file demos or demos requiring C++
# Note: FtPthread.o and USB_Device.o are compiled together
FtPthread-dynamic: FtPthread.o USB_Device.o
	$(CC) $(COMMON_CFLAGS) -o $@ $^ -lstdc++ $(LIBS)

# Demos that need the C++ standard library explicitly
streamer-dynamic: streamer.o
	$(CXX) $(COMMON_CFLAGS) -o $@ $^ -lstdc++ $(LIBS)

asyncstreamer-dynamic: asyncstreamer.o
	$(CC) $(COMMON_CFLAGS) -o $@ $^ -lstdc++ $(LIBS)

async-loopback-dynamic: async-loopback.o
	$(CC) $(COMMON_CFLAGS) -o $@ $^ -lstdc++ $(LIBS)

FTD3xx_Notification-dynamic: FTD3xx_Notification.o
	$(CC) $(COMMON_CFLAGS) -o $@ $^ -lstdc++ $(LIBS)

# Clean rule
clean:
	rm -f *.o $(addsuffix -dynamic,$(DEMOS))