#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>

struct vplat_info {
    unsigned int 	buf_max_size;
    unsigned int 	buffer_size;
    unsigned int 	period_size;
    char 			*addr;
	unsigned int 	buf_pos;
    unsigned int 	be_running;
};

static struct vplat_info playback_info;
static struct vplat_info capture_info;

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
						SNDRV_PCM_FMTBIT_S8,
	.rates			= SNDRV_PCM_RATE_8000_192000 | 
						SNDRV_PCM_RATE_KNOT,
	.rate_min			= 8000,
	.rate_max			= 192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= 128*1024,
	.period_bytes_min	= PAGE_SIZE,
	.period_bytes_max	= PAGE_SIZE*2,
	.periods_min		= 1,
	.periods_max		= 8,
	.fifo_size		= 32,
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



static int vplat_pcm_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
    //int ret;

    /* 设置属性 */
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &vplat_pcm_hardware);
    

	return 0;
}

static int vplat_pcm_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	/* 注销定时器 */

	return 0;
}

static int vplat_pcm_ioctl(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, unsigned int cmd, void *arg)
{
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int vplat_pcm_hw_params(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, 
		struct snd_pcm_hw_params *params) 
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long totbytes = params_buffer_bytes(params);
    

    /* pcm_new分配了很大的BUFFER
     * params决定使用多大
     */
	runtime->dma_bytes            = totbytes;
	
	/* save config */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		playback_info.buffer_size = totbytes;
		playback_info.period_size = params_period_bytes(params);
	} else {
		capture_info.buffer_size = totbytes;
		capture_info.period_size = params_period_bytes(params);
	}
	
	/* 根据params设置DMA */
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int vplat_pcm_prepare(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
    /* 准备数据传输 */

    /* 复位各种状态信息 */
    playback_info.buf_pos = 0;
    playback_info.be_running = 0;
	
	capture_info.buf_pos = 0;
    capture_info.be_running = 0;
    
    /* 加载第1个period */
    

	return 0;
}

/* 根据cmd启动或停止数据传输 */
static int vplat_pcm_trigger(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			/* 启动定时器, 模拟数据传输 */
			playback_info.be_running = 1;
			
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			/* 停止定时器 */
			playback_info.be_running = 0;
			
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
			capture_info.be_running = 1;
			
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			/* catpure停止接收数据 */
			capture_info.be_running = 0;
			
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
		playback_info.addr = vmalloc(vplat_pcm_hardware.buffer_bytes_max);
		if(IS_ERR(playback_info.addr)){
			printk(KERN_ERR"[ERROR]Couldn't alloc playback buffer!!!\n");
			return -1;
		}
		
		playback_info.buf_max_size = vplat_pcm_hardware.buffer_bytes_max;
		
		substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
		buf = &substream->dma_buffer;

    	buf->dev.type = SNDRV_DMA_TYPE_DEV;
    	buf->dev.dev = pcm->card->dev;
    	buf->private_data = NULL;
        buf->area = playback_info.addr;
        buf->bytes = playback_info.buf_max_size;
		
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		capture_info.addr = vmalloc(vplat_pcm_hardware.buffer_bytes_max);
		if(IS_ERR(capture_info.addr)){
			printk(KERN_ERR"[ERROR]Couldn't alloc capture buffer!!!\n");
			vfree(playback_info.addr);
			return -1;
		}
		
		capture_info.buf_max_size = vplat_pcm_hardware.buffer_bytes_max;
		
		substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
		buf = &substream->dma_buffer;

    	buf->dev.type = SNDRV_DMA_TYPE_DEV;
    	buf->dev.dev = pcm->card->dev;
    	buf->private_data = NULL;
        buf->area = capture_info.addr;
        buf->bytes = capture_info.buf_max_size;	
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

		vfree(buf->area);
		buf->area = NULL;
	}
}

// static struct snd_pcm_ops vplat_pcm_ops = {
// 	.open		= vplat_pcm_open,
// 	.close		= vplat_pcm_close,
// 	.ioctl		= snd_pcm_lib_ioctl,
// 	.hw_params	= vplat_pcm_hw_params,
// 	.prepare    = vplat_pcm_prepare,
// 	.trigger	= vplat_pcm_trigger,
// 	.pointer	= vplat_pcm_pointer,
// 	//.mmap		= vplat_pcm_mmap,
// };

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

static struct snd_soc_component_driver vplat_soc_drv = {
	.name = "vplat",
	// 在创建 PCM runtime 时分配 buffer。
	.pcm_construct = vplat_pcm_construct,
	// 在销毁时释放 buffer。
	.pcm_destruct = vplat_pcm_destruct,
	.open		= vplat_pcm_open,
	.close		= vplat_pcm_close,
	.ioctl      = vplat_pcm_ioctl,
	.hw_params	= vplat_pcm_hw_params,
	.prepare    = vplat_pcm_prepare,
	.trigger	= vplat_pcm_trigger,
	.pointer	= vplat_pcm_pointer,
	//.mmap		= vplat_pcm_mmap,
};


static int vplat_probe(struct platform_device *pdev) {
	int ret = 0;
	
	printk("-----%s----\n",__func__);
	
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
	printk("-----%s----\n",__func__);

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
