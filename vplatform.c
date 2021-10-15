#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>


static const struct snd_soc_component_driver plat_cpudai_component = {
	.name = "plat-cpudai",
};

static struct snd_soc_dai_driver plat_cpudai_dai = {
	.name	= "plat-cpudai",
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

static struct snd_soc_platform_driver plat_soc_drv = {
	//.ops		= &s3c2440_dma_ops,
	//.pcm_new	= s3c2440_dma_new,
	//.pcm_free	= s3c2440_dma_free,
};


static int plat_probe(struct platform_device *pdev) {
	int ret = 0;
	
	printk("-----%s----\n",__func__);
	
	ret = snd_soc_register_component(&pdev->dev, &plat_cpudai_component,
					&plat_cpudai_dai, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register CPU DAI: %d\n", ret);
		ret = -EBUSY;
		return ret;
	}
	
	
	ret = snd_soc_register_platform(&pdev->dev, &plat_soc_drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register platform: %d\n", ret);
		ret = -EBUSY;
		return ret;
	}
	
	return ret;
}

static int plat_remove(struct platform_device *pdev){
	printk("-----%s----\n",__func__);

	return 0;
}

static void plat_pdev_release(struct device *dev)
{
}

static struct platform_device plat_pdev = {
	.name			= "vplat",
	.dev.release	= plat_pdev_release,
};

static struct platform_driver plat_pdrv = {
	.probe		= plat_probe,
	.remove		= plat_remove,
	.driver		= {
		.name	= "vplat",
	},
};

static int __init plat_init(void)
{
	int ret;

	ret = platform_device_register(&plat_pdev);
	if (ret)
		return ret;

	ret = platform_driver_register(&plat_pdrv);
	if (ret)
		platform_device_unregister(&plat_pdev);

	return ret;
}

static void __exit plat_exit(void)
{
	platform_driver_unregister(&plat_pdrv);
	platform_device_unregister(&plat_pdev);
}



module_init(plat_init);
module_exit(plat_exit);
MODULE_LICENSE("GPL");
