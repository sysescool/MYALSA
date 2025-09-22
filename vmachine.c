#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

static int my_card_init(struct snd_soc_pcm_runtime *rtd)
{
	printk("%s,line:%d\n",__func__,__LINE__);
	return 0;
}

static int my_card_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params) {

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	unsigned int freq;
	//int ret;
	int stream_flag;
	
	printk("%s,rate:%d\n",__func__,params_rate(params));
	
	switch (params_rate(params)) {
	case	8000:
	case	12000:
	case	16000:
	case	24000:
	case	32000:
	case	48000:
	case	96000:
	case	192000:
		freq = 24576000;
		break;
	case	11025:
	case	22050:
	case	44100:
		freq = 22579200;
		break;
	default:
		dev_err(card->dev, "invalid rate setting\n");
		return -EINVAL;
	}

	/* the substream type: 0->playback, 1->capture */
	stream_flag = substream->stream;

	/* 通过snd_soc_dai_set_sysclk()设置codec的sysclk*/
	//if (freq == 22579200) {
	//	if (stream_flag == 0) {
	//		ret = snd_soc_dai_set_sysclk(codec_dai, 0, freq, 0);
	//		if (ret < 0) {
	//			dev_err(card->dev, "sndcodec:set codec dai sysclk faided, freq:%d\n", freq);
	//			return ret;
	//		}
	//	}
	//}
	

	return 0;
}

static struct snd_soc_ops my_card_ops = {
	.hw_params = my_card_hw_params,
};

SND_SOC_DAILINK_DEFS(my_card,
	DAILINK_COMP_ARRAY(COMP_CPU("vplat.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("vcodec.0", "vcodec_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("vplat.0")));

static struct snd_soc_dai_link my_card_dai_link[] = {
	{
		.name		= "my-codec",
		.stream_name	= "MY-CODEC",
		SND_SOC_DAILINK_REG(my_card),
		.init		= my_card_init,
		.ops		= &my_card_ops,
	},
};


static struct snd_soc_card snd_soc_my_card = {
	.name			= "my-codec",
	.owner			= THIS_MODULE,
	.dai_link		= my_card_dai_link,
	.num_links		= ARRAY_SIZE(my_card_dai_link),
};


static int vmachine_probe(struct platform_device *pdev) {
	int ret = 0;
	struct snd_soc_card *card = &snd_soc_my_card;
	
	printk("%s,line:%d\n",__func__,__LINE__);
	
	/* register the soc card */
	card->dev = &pdev->dev;
	
	ret = snd_soc_register_card(card);
	if (ret < 0) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		return -1;
	}
	platform_set_drvdata(pdev,card);
	
	return ret;
}

static int vmachine_remove(struct platform_device *pdev){
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	printk("%s,line:%d\n",__func__,__LINE__);
	snd_soc_unregister_card(card);
	return 0;
}

static void vmachine_pdev_release(struct device *dev)
{
}

static struct platform_device vmachine_pdev = {
	.name			= "vmachine",
	.dev.release	= vmachine_pdev_release,
};

static struct platform_driver vmachine_pdrv = {
	.probe		= vmachine_probe,
	.remove		= vmachine_remove,
	.driver		= {
		.name	= "vmachine",
	},
};

static int __init vmachine_init(void)
{
	int ret;

	ret = platform_device_register(&vmachine_pdev);
	if (ret)
		return ret;

	ret = platform_driver_register(&vmachine_pdrv);
	if (ret)
		platform_device_unregister(&vmachine_pdev);

	return ret;
}

static void __exit vmachine_exit(void)
{
	platform_driver_unregister(&vmachine_pdrv);
	platform_device_unregister(&vmachine_pdev);
}

module_init(vmachine_init);
module_exit(vmachine_exit);
MODULE_LICENSE("GPL");
