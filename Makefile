
TARGET_MODULE := My_proc

obj-m := $(TARGET_MODULE).o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: 
	gcc -o MT_matrix MT_matrix.c -lpthread
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null


check: all
	$(MAKE) unload
	$(MAKE) load
	./MT_matrix
	$(MAKE) unload