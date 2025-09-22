# KERN_DIR = /home/vbox/workspace/qemu/100ask_imx6ull-qemu/linux-4.9.88
KERN_DIR = /usr/src/linux-headers-6.8.0-83-generic

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= vplatform.o
obj-m	+= vcodec.o
obj-m	+= vmachine.o
