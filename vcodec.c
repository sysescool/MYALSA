#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

static int vcodec_probe(struct snd_soc_component *component)
{
	//int ret;
	printk("-----%s----\n",__func__);

	return 0;
}

static struct snd_soc_component_driver soc_vcodec_drv = {
	.name = "vcodec",
	.probe = vcodec_probe,
	//.remove = vcodec_remove,
	//.read = vcodec_read,
	//.write = vcodec_write,
	//.ignore_pmdown_time = 1,
};

static struct snd_soc_dai_driver vcodec_dai[] = {
	{
		.name	= "vcodec_dai",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates	= SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE,
		},
		//.ops = &vcodec_dai_ops,
	},
};

static int codec_probe(struct platform_device *pdev) {
	int ret = 0;
	
	printk("-----%s----\n",__func__);
	
	ret = snd_soc_register_component(&pdev->dev, &soc_vcodec_drv,
				vcodec_dai, ARRAY_SIZE(vcodec_dai));
	if (ret < 0) {
		dev_err(&pdev->dev, "register codec failed\n");
		return -1;
	}
	
	return ret;
}

static int codec_remove(struct platform_device *pdev){
	printk("-----%s----\n",__func__);

	return 0;
}

static void codec_pdev_release(struct device *dev)
{
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

static int __init codec_init(void)
{
	int ret;

	ret = platform_device_register(&codec_pdev);
	if (ret)
		return ret;

	ret = platform_driver_register(&codec_pdrv);
	if (ret)
		platform_device_unregister(&codec_pdev);

	return ret;
}

static void __exit codec_exit(void)
{
	platform_driver_unregister(&codec_pdrv);
	platform_device_unregister(&codec_pdev);
}



module_init(codec_init);
module_exit(codec_exit);
MODULE_LICENSE("GPL");
