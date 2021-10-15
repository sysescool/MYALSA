KERN_DIR = /home/vbox/workspace/qemu/100ask_imx6ull-qemu/linux-4.9.88

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= vplatform.o
obj-m	+= vcodec.o
obj-m	+= vmachine.o
