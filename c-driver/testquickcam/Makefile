#LINUX_DIR = /usr/src/linux
# 
#MODULE_INC= -I$(LINUX_DIR)/include -include $(LINUX_DIR)/include/linux/config.h
#MODULE_DEFS:= $(shell [ -f /usr/include/linux/modversions.h ] && echo -DEXPORT_SYMTAB -DMODVERSIONS -include /usr/include/linux/modversions.h )
#MODULE_OPT = -O3
#MODULE_OPT_WARN =  -Wall -Wstrict-prototypes -fomit-frame-pointer  -pipe
#MODULE_CFLAGS = $(MODULE_INC) $(MODULE_OPT) $(MODULE_OPT_WARN) $(MODULE_DEFS)
all: testquickcam.o

clean:
	rm testquickcam -f

testquickcam.o: testquickcam.c

.c.o:
	$(CC) -o testquickcam $<

