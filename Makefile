#代码路径
#KERN_DIR = /home/vbox/workspace/qemu/100ask_imx6ull-qemu/linux-4.9.88
KERN_DIR = /usr/src/linux-headers-6.8.0-83-generic

all: modules play_to_cap

modules:
	make -C $(KERN_DIR) M=`pwd` modules

play_to_cap:
	gcc play_to_cap.c -o play_to_cap -lasound 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f play_to_cap

obj-m	+= vplatform.o
obj-m	+= vcodec.o
obj-m	+= vmachine.o
