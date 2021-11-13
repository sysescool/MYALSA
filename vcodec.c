#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define DAC_VOL_L		8
#define DAC_VOL_R		0

enum reg {
	VCODEC_DAC_VOL_CTRL,
	VCODEC_CTRL_NUM
};

static u32 reg_data[VCODEC_CTRL_NUM];



static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -11925, 75, 0);

static const struct snd_kcontrol_new vcodec_codec_controls[] = {
	SOC_DOUBLE_TLV("DAC volume", VCODEC_DAC_VOL_CTRL, DAC_VOL_L, DAC_VOL_R,
		       0xFF, 0, dac_vol_tlv),
};

static int vcodec_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	printk("-%s,line:%d\n",__func__,__LINE__);
	
	/* 1.加controls */
	ret = snd_soc_add_codec_controls(codec, vcodec_codec_controls,
					ARRAY_SIZE(vcodec_codec_controls));
	if(ret < 0) {
		printk(KERN_ERR"vcodec add controls error!!!\n");
		return ret;
	}
	
	/* 2.初始化codec */

	return ret;
}

static int vcodec_remove(struct snd_soc_codec *codec)
{
	printk("-%s,line:%d\n",__func__,__LINE__);
	return 0;
}

static unsigned int vcodec_reg_read(struct snd_soc_codec *codec,
					unsigned int reg)
{
	
	if(reg >= VCODEC_CTRL_NUM) {
		printk(KERN_ERR"%s: parameter error!!!\n",__func__);
		return 0;
	}
	
	printk("-%s,line:%d,reg_data[%d] = 0x%x\n",__func__,__LINE__,reg,reg_data[reg]);

	return reg_data[reg];
}

static int vcodec_reg_write(struct snd_soc_codec *codec,
				unsigned int reg, unsigned int val)
{
	printk("-%s,line:%d,reg=0x%x,val=0x%x\n",__func__,__LINE__,reg,val);
	if(reg >= VCODEC_CTRL_NUM) {
		printk(KERN_ERR"%s: parameter error!!!\n",__func__);
		return -1;
	}
	
	reg_data[reg] = val;

	return 0;
};

static struct snd_soc_codec_driver soc_vcodec_drv = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.read = vcodec_reg_read,
	.write = vcodec_reg_write,
	.ignore_pmdown_time = 1,
};


static int vcodec_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai) {
	printk("-%s,line:%d\n",__func__,__LINE__);
	return 0;
}

static int vcodec_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
    /* 根据params的值,设置codec的寄存器 
     * 比如时钟设置,格式,采样率等
     */
	
	printk("-%s,line:%d\n",__func__,__LINE__);

    return 0;
}

static void vcodec_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai) {
	printk("-%s,line:%d\n",__func__,__LINE__);				
}

static int vcodec_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			printk("-%s: playback start\n",__func__);
		} else {
			printk("-%s: catpure start\n",__func__);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			printk("-%s:playback stop\n",__func__);
		} else {
			printk("-%s:catpure stop\n",__func__);
		}

		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int vcodec_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai) {
	printk("-%s,line:%d\n",__func__,__LINE__);
	return 0;
}

static const struct snd_soc_dai_ops vcodec_dai_ops = {
	.startup		= vcodec_startup,
	.hw_params		= vcodec_hw_params,
	.prepare		= vcodec_prepare,
	.trigger		= vcodec_trigger,
	.shutdown		= vcodec_shutdown,
};


static struct snd_soc_dai_driver vcodec_dai[] = {
	{
		.name	= "vcodec_dai",
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
		.ops = &vcodec_dai_ops,
	},
};

static int codec_probe(struct platform_device *pdev) {
	int ret = 0;
	
	printk("-%s,line:%d\n",__func__,__LINE__);
	
	ret = snd_soc_register_codec(&pdev->dev, &soc_vcodec_drv,
				vcodec_dai, ARRAY_SIZE(vcodec_dai));
	if (ret < 0) {
		dev_err(&pdev->dev, "register codec failed\n");
		return -1;
	}
	
	return ret;
}

static int codec_remove(struct platform_device *pdev){
	printk("-%s,line:%d\n",__func__,__LINE__);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static void codec_pdev_release(struct device *dev) {
}

static struct platform_device codec_pdev = {
	.name			= "vcodec",
	.dev.release	= codec_pdev_release,
};

static struct platform_driver codec_pdrv = {
	.probe		= codec_probe,
	.remove		= codec_remove,
	.driver		= {
		.name	= "vcodec",
	},
};

static int __init codec_init(void) {
	int ret;

	ret = platform_device_register(&codec_pdev);
	if (ret)
		return ret;

	ret = platform_driver_register(&codec_pdrv);
	if (ret)
		platform_device_unregister(&codec_pdev);

	return ret;
}

static void __exit codec_exit(void) {
	platform_driver_unregister(&codec_pdrv);
	platform_device_unregister(&codec_pdev);
}

module_init(codec_init);
module_exit(codec_exit);
MODULE_LICENSE("GPL");
