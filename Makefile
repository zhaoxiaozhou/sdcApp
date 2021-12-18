CC = aarch64-himix100-linux-gcc

SRCS = $(wildcard *.c)

OBJS = $(SRCS:.c = .o)
#OBJS = $(patsubst %.c,%.o,$(wildcard *.c))

INCDIR := -I./inc -I./include

SRCDIR := ./src
SRC_OBJS := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/*.c))
LIBS = -L./ -L./lib

LDFLAG = -g -Wl,-z,relro,-z,noexecstack,--disable-new-dtags,-rpath,/lib/:/usr/lib/:/usr/app/lib

CCFLAGS = -g -Wall -O0

LDFLAG += -fstack-protector --param ssp-buffer-size=4 -Wfloat-equal -Wshadow -Wformat=2

animal : $(SRC_OBJS)

	$(CC) main.o queue.o sdc_os_api.o -o $@ $(INCDIR) $(LIBS) $(LDFLAG) -lrt -lm -lsecurec -pthread

$(SRC_OBJS) : $(wildcard $(SRCDIR)/*.c)

	$(CC) -c $^ $(INCDIR) $(CCFLAGS) -lpthread

clean:

	rm *.o

.PHONY:clean
