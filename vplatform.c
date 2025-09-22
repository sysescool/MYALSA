#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>

#include <linux/timer.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>

struct vplat_info {
    unsigned int 	buf_max_size;
    unsigned int 	buffer_size;
    unsigned int 	period_size;
    char 			*addr;
	unsigned int 	buf_pos;
    unsigned int 	is_running;
	struct snd_pcm_substream *substream;
};

static struct vplat_info playback_info;
static struct vplat_info capture_info;

static struct timer_list vtimer;
static void work_function(struct work_struct *work);
DECLARE_WORK(vplat_work,work_function);

#define DUMP_PLAYBACK
#ifdef DUMP_PLAYBACK
static struct file *fp;
#define DUMP_DIR "/home/playback.pcm"
#endif

static u64 dma_mask = DMA_BIT_MASK(32);

static const struct snd_pcm_hardware vplat_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |	//数据的排列方式（左右左右左右还是左左左右右右）
						SNDRV_PCM_INFO_BLOCK_TRANSFER |
						SNDRV_PCM_INFO_MMAP |
						SNDRV_PCM_INFO_MMAP_VALID |
						SNDRV_PCM_INFO_PAUSE |
						SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |	//所支持的音频数据格式
						SNDRV_PCM_FMTBIT_U16_LE |
						SNDRV_PCM_FMTBIT_U8 |
						SNDRV_PCM_FMTBIT_S8 |
						SNDRV_PCM_FMTBIT_S32_LE,
	.rates			= SNDRV_PCM_RATE_8000_192000 | 
						SNDRV_PCM_RATE_KNOT,
	.rate_min			= 8000,
	.rate_max			= 192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= 1024 * 256,
	.period_bytes_min	= 256,
	.period_bytes_max	= 1024 * 128,
	.periods_min		= 1,
	.periods_max		= 8,
	.fifo_size			= 128,
};


static const struct snd_soc_component_driver vplat_cpudai_component = {
	.name = "vplat-cpudai",
};

static struct snd_soc_dai_driver vplat_cpudai_dai = {
	.name	= "vplat-cpudai",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000 |
			SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE	|
			SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000 |
			SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE	|
			SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops	= NULL,
};

#ifdef DUMP_PLAYBACK
static struct file *vfs_open_file(char *file_path)
{
	struct file *fp;

	fp = filp_open(file_path, O_RDWR | O_APPEND | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		printk(KERN_ERR"open %s failed!, ERR NO is %ld.\n", file_path,
		       (long)fp);
	}
	return fp;
}

static int vfs_write_file_append(struct file *fp, char *buf, size_t len) {
	mm_segment_t old_fs;
	static loff_t pos = 0;
	int buf_len;

	if (IS_ERR_OR_NULL(fp)) {
		printk(KERN_ERR"write file error, fp is null!");
		return -1;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	buf_len = vfs_write(fp, buf, len, &pos);
	set_fs(old_fs);
	

	if (buf_len < 0)
		return -1;
	if (buf_len != len)
		printk(KERN_ERR"buf_len = %x, len = %pa\n", buf_len, &len);
	pos += buf_len;
	return buf_len;
}

static int vfs_close_file(struct file *fp)
{
	if (IS_ERR(fp)) {
		printk(KERN_ERR"colse file failed,fp is invaild!\n");
		return -1;
	} else {
		filp_close(fp, NULL);
		return 0;
	}
}
#endif

static int load_buff_period(void) {
	struct snd_pcm_substream *cp_substream = capture_info.substream;
	int size = 0;
	
	if(capture_info.addr == NULL) {
		printk(KERN_ERR"catpure addr error!!!\n");
		return -1;
	}

	if (playback_info.is_running) {
		if(capture_info.period_size != playback_info.period_size) {
			printk(KERN_ERR"capture_info.period_size(%d) != playback_info.period_size(%d)\n",
					capture_info.period_size,playback_info.period_size);
		}
		
		size = capture_info.period_size <= playback_info.period_size ?
				capture_info.period_size :
				playback_info.period_size;
		
		//复制playback的一帧数据到catpure
		memcpy(capture_info.addr+capture_info.buf_pos,
				playback_info.addr+playback_info.buf_pos,
				size);
	} else {
		memset(capture_info.addr+capture_info.buf_pos,0x00,capture_info.period_size);
	}
	
	//更新capture当前buffer指针位置
	capture_info.buf_pos += capture_info.period_size;
	if (capture_info.buf_pos >= capture_info.buffer_size)
		capture_info.buf_pos = 0;
	
	snd_pcm_period_elapsed(cp_substream);
	return 0;
}

static void work_function(struct work_struct *work){
	
	struct snd_pcm_substream *pb_substream = playback_info.substream;
	
	//printk("%s,line:%d\n",__func__,__LINE__);
#ifdef DUMP_PLAYBACK
	if(playback_info.is_running) {
		fp = vfs_open_file(DUMP_DIR);
		vfs_write_file_append(fp,
						playback_info.addr+playback_info.buf_pos,
						playback_info.period_size);
		vfs_close_file(fp);
	}
#endif
	
	if (capture_info.is_running) {
		load_buff_period();
	}
        
    // 更新状态信息
	if(playback_info.is_running){
		playback_info.buf_pos += playback_info.period_size;
		if (playback_info.buf_pos >= playback_info.buffer_size)
			playback_info.buf_pos = 0;
		
		// 更新hw_ptr等信息,
		// 并且判断:如果buffer里没有数据了,则调用trigger来停止DMA 
		snd_pcm_period_elapsed(pb_substream); 
	}

    if (playback_info.is_running || capture_info.is_running) {
         
        //再次启动定时器
        mod_timer(&vtimer, jiffies + HZ/10);
    }
}

#ifdef setup_timer
static void vplat_timer_function(unsigned long data) {
#else	
static void vplat_timer_function(struct timer_list *t) {
#endif
	
	schedule_work(&vplat_work);
}

static void start_timer(void) {
#ifdef setup_timer
	setup_timer(&vtimer, vplat_timer_function, 0);
#else
	timer_setup(&vtimer, vplat_timer_function, 0);
#endif
	vtimer.expires = jiffies + HZ/10;
	add_timer(&vtimer);
}


static int vplat_pcm_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	printk("%s,line:%d\n",__func__,__LINE__);

    // 设置属性
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &vplat_pcm_hardware);
	
	//可以在这里注册中断
    

	return 0;
}

static int vplat_pcm_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	printk("%s,line:%d\n",__func__,__LINE__);

	return 0;
}

static int vplat_pcm_hw_params(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, 
		struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long totbytes = params_buffer_bytes(params);
    
	//printk("%s,line:%d\n",__func__,__LINE__);

    /* pcm_new分配了很大的BUFFER
     * params决定使用多大
     */
	runtime->dma_bytes            = totbytes;
	
	// 保存config
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		playback_info.buffer_size = totbytes;
		playback_info.period_size = params_period_bytes(params);
	} else {
		capture_info.buffer_size = totbytes;
		capture_info.period_size = params_period_bytes(params);
	}
	
	//设置runtime->dma_area
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

/* 准备数据传输 */
static int vplat_pcm_prepare(struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	//printk("%s,line:%d\n",__func__,__LINE__);
    
    /* 复位各种状态信息 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		playback_info.buf_pos = 0;
		playback_info.is_running = 0;
	} else {
		capture_info.buf_pos = 0;
		capture_info.is_running = 0;
		
		/* 加载第1个period */
		//load_buff_period();
    }
    

	return 0;
}

/* 根据cmd启动或停止数据传输 */
static int vplat_pcm_trigger(struct snd_soc_component *component,
	struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	static u8 is_timer_run = 0;
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			/* 启动定时器, 模拟数据传输 */
			printk("playback running...\n");
			playback_info.is_running = 1;
			if(!is_timer_run) {
				is_timer_run = 1;
				start_timer();
			}
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			/* 停止定时器 */
			printk("playback stop...\n");
			playback_info.is_running = 0;
			if(!capture_info.is_running){
				is_timer_run = 0;
				del_timer(&vtimer);
			}
			break;

		default:
			ret = -EINVAL;
			break;
		}
	} else {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			/* catpure开始接收数据 */
			
			printk("capture running...\n");
			capture_info.is_running = 1;
			if(!is_timer_run) {
				is_timer_run = 1;
				start_timer();
			}
			
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			/* catpure停止接收数据 */
			printk("capture stop...\n");
			capture_info.is_running = 0;
			if(!playback_info.is_running){
				is_timer_run = 0;
				del_timer(&vtimer);
			}
			break;

		default:
			ret = -EINVAL;
			break;
		}
	}


	return ret;
}

/* 返回结果是frame */
static snd_pcm_uframes_t vplat_pcm_pointer(struct snd_soc_component *component, 
	struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return bytes_to_frames(substream->runtime, playback_info.buf_pos);
	else {
		return bytes_to_frames(substream->runtime, capture_info.buf_pos);
	}
}




static int vplat_pcm_new(struct snd_soc_pcm_runtime *rtd) {
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {

		playback_info.buf_max_size = vplat_pcm_hardware.buffer_bytes_max;
		substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
		playback_info.substream = substream;
		buf = &substream->dma_buffer;
		
		buf->area = dma_alloc_coherent(pcm->card->dev, playback_info.buf_max_size,
					&buf->addr, GFP_KERNEL);
		if (!buf->area) {
			printk(KERN_ERR"plaback alloc dma error!!!\n");
			return -ENOMEM;
		}

    	buf->dev.type = SNDRV_DMA_TYPE_DEV;
    	buf->dev.dev = pcm->card->dev;
    	buf->private_data = NULL;
        buf->bytes = playback_info.buf_max_size;
		
		playback_info.addr = buf->area;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		
		capture_info.buf_max_size = vplat_pcm_hardware.buffer_bytes_max;
		
		substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
		capture_info.substream = substream;
		buf = &substream->dma_buffer;
		
		buf->area = dma_alloc_coherent(pcm->card->dev, capture_info.buf_max_size,
					&buf->addr, GFP_KERNEL);
		if (!buf->area) {
			printk(KERN_ERR"catpure alloc dma error!!!\n");
			return -ENOMEM;
		}

    	buf->dev.type = SNDRV_DMA_TYPE_DEV;
    	buf->dev.dev = pcm->card->dev;
    	buf->private_data = NULL;
        buf->bytes = capture_info.buf_max_size;	
		
		capture_info.addr = buf->area;
	}

	return ret;
}


static void vplat_pcm_free_buffers(struct snd_pcm *pcm){
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_coherent(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
		buf->area = NULL;
	}
}

//static int vplat_pcm_copy(struct snd_pcm_substream *substream, 
//				int a,snd_pcm_uframes_t hwoff, 
//				void __user *buf, snd_pcm_uframes_t frames) {
//	
//	int ret = 0;
//	char *hwbuf;
//	struct snd_pcm_runtime *runtime = substream->runtime;
//	//printk("%s,line:%d\n",__func__,__LINE__);
//	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
//		hwbuf = runtime->dma_area + frames_to_bytes(runtime, hwoff);
//		if (copy_from_user(hwbuf, buf,
//				frames_to_bytes(runtime, frames))) {
//			return -EFAULT;
//		}
//		
//	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
//		hwbuf = runtime->dma_area + frames_to_bytes(runtime, hwoff);
//		if (copy_to_user(buf, hwbuf, frames_to_bytes(runtime, frames)))
//			return -EFAULT;
//	}
//
//	return ret;
//}

static int vplat_pcm_mmap(struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = NULL;
	printk("%s,line:%d\n",__func__,__LINE__);
	if (substream->runtime != NULL) {
		runtime = substream->runtime;

		return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					     runtime->dma_area,
					     runtime->dma_addr,
					     runtime->dma_bytes);
	} else {
		return -1;
	}

}

/* New API compatible PCM functions */
static int vplat_pcm_construct(struct snd_soc_component *component,
	struct snd_soc_pcm_runtime *rtd)
{
	return vplat_pcm_new(rtd);
}

static void vplat_pcm_destruct(struct snd_soc_component *component,
	struct snd_pcm *pcm)
{
	vplat_pcm_free_buffers(pcm);
}

static int vplat_pcm_ioctl(struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	unsigned int cmd, void *arg)
{
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static struct snd_soc_component_driver vplat_soc_drv = {
	.pcm_construct	= vplat_pcm_construct,
	.pcm_destruct	= vplat_pcm_destruct,
	.open		= vplat_pcm_open,
	.close		= vplat_pcm_close,
	.ioctl		= vplat_pcm_ioctl,
	.hw_params	= vplat_pcm_hw_params,
	.prepare    = vplat_pcm_prepare,
	.trigger	= vplat_pcm_trigger,
	.pointer	= vplat_pcm_pointer,
	.mmap		= vplat_pcm_mmap,
	//.copy		= vplat_pcm_copy,
};


static int vplat_probe(struct platform_device *pdev) {
	int ret = 0;
	
	printk("%s,line:%d\n",__func__,__LINE__);
	
	ret = snd_soc_register_component(&pdev->dev, &vplat_cpudai_component,
		&vplat_cpudai_dai, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register CPU DAI: %d\n", ret);
		ret = -EBUSY;
		return ret;
	}
	
	ret = snd_soc_register_component(&pdev->dev, &vplat_soc_drv, NULL, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register platform: %d\n", ret);
		ret = -EBUSY;
		return ret;
	}
	
	return ret;
}

static int vplat_remove(struct platform_device *pdev){
	printk("%s,line:%d\n",__func__,__LINE__);
	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static void vplat_pdev_release(struct device *dev)
{
}

static struct platform_device vplat_pdev = {
	.name			= "vplat",
	.dev.release	= vplat_pdev_release,
};

static struct platform_driver vplat_pdrv = {
	.probe		= vplat_probe,
	.remove		= vplat_remove,
	.driver		= {
		.name	= "vplat",
	},
};

static int __init vplat_init(void)
{
	int ret;

	ret = platform_device_register(&vplat_pdev);
	if (ret)
		return ret;

	ret = platform_driver_register(&vplat_pdrv);
	if (ret)
		platform_device_unregister(&vplat_pdev);

	return ret;
}

static void __exit vplat_exit(void)
{
	platform_driver_unregister(&vplat_pdrv);
	platform_device_unregister(&vplat_pdev);
}



module_init(vplat_init);
module_exit(vplat_exit);
MODULE_LICENSE("GPL");
