
# 编译

```bash
make
```


# 如何使用编译出来的模块

## 加载模块（按顺序）

```bash
# 1. 先加载平台驱动
sudo insmod vplatform.ko

# 2. 再加载编解码器驱动
sudo insmod vcodec.ko

# 3. 最后加载机器驱动（连接前两个）
sudo insmod vmachine.ko
```

## 检查模块状态

```bash
# 查看已加载的模块
lsmod | grep -E "(vplatform|vcodec|vmachine)"
# vmachine               16384  0
# vcodec                 12288  1
# vplatform              12288  2
# snd_soc_core          442368  7 soundwire_intel,snd_sof,vcodec,snd_sof_intel_hda_common,snd_soc_hdac_hda,vmachine,vplatform

# 查看音频设备
aplay -l
# card 3: mycodec [my-codec], device 0: MY-CODEC vcodec_dai-0 []
#  Subdevices: 1/1
#  Subdevice #0: subdevice #0
arecord -l
# card 3: mycodec [my-codec], device 0: MY-CODEC vcodec_dai-0 []
#   Subdevices: 1/1
#   Subdevice #0: subdevice #0

# 查看ALSA音频卡
cat /proc/asound/cards
#  3 [mycodec        ]: my-codec - my-codec
#                      ASUSTeKCOMPUTERINC.-SystemProductName-SystemVersion-PRIMEB660M_KD4
```

## 卸载模块（按相反顺序）

```bash
# 1. 先卸载机器驱动
sudo rmmod vmachine

# 2. 再卸载编解码器驱动
sudo rmmod vcodec

# 3. 最后卸载平台驱动
sudo rmmod vplatform
```

## 测试音频功能

```bash
# 播放测试（如果有音频文件）
aplay -D hw:my-codec,0 test.wav

# 录制测试
arecord -D hw:my-codec,0 -f S16_LE -r 44100 -c 2 test.wav
```

## 查看内核日志
```bash
# 查看模块加载时的调试信息
dmesg | tail -20
# [ 9343.486190] -----plat_probe----
# [ 9349.558149] -----codec_probe----
# [ 9355.746272] -----vmachine_probe----
# [ 9355.746327] -----vcodec_probe----
# [ 9355.746329] debugfs: Directory 'vplat.0' with parent 'my-codec' already present!
```