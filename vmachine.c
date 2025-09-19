#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>



//static struct snd_soc_ops my_card_ops = {
//	.hw_params = my_card_hw_params,
//};

SND_SOC_DAILINK_DEFS(my_card,
	DAILINK_COMP_ARRAY(COMP_CPU("vplat.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("vcodec.0", "vcodec_dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("vplat.0")));

static struct snd_soc_dai_link my_card_dai_link[] = {
	{
		.name		= "my-codec",
		.stream_name	= "MY-CODEC",
		SND_SOC_DAILINK_REG(my_card),
//		.init		= my_card_init,
//		.ops		= &my_card_ops,
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
	
	printk("-----%s----\n",__func__);
	
	/* register the soc card */
	card->dev = &pdev->dev;
	
	ret = snd_soc_register_card(card);
	if (ret < 0) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		return -1;
	}
	
	return ret;
}

static int vmachine_remove(struct platform_device *pdev){
	printk("-----%s----\n",__func__);

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
