// SPDX-License-Identifier: GPL-1.0-or-later
/*
 *  AES67 soundcard
 *
 *  */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/initval.h>

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;
static int pcm_devs[SNDRV_CARDS] = { [0 ...(SNDRV_CARDS - 1)] = 1 };
static int pcm_substreams[SNDRV_CARDS] = { [0 ...(SNDRV_CARDS - 1)] = 8 };

static struct platform_device *devices[SNDRV_CARDS];

#define CARD_NAME "AES67 Virtual Soundcard"

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for " CARD_NAME " soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for " CARD_NAME " soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable " CARD_NAME " soundcard.");
module_param_array(pcm_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_devs, "PCM devices # (0-4) for dummy driver.");
module_param_array(pcm_substreams, int, NULL, 0444);
MODULE_PARM_DESC(pcm_substreams, "PCM substreams # (1-128) for dummy driver.");

MODULE_AUTHOR("Preston Baxter <preston@preston-baxter.xom>");
MODULE_DESCRIPTION("AES67 Virtual Soundcard");
MODULE_LICENSE("GPL");

/* Definistion of AES67 Virtual SoundCard */
struct aes67 {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct device *dev;
};

//Forward declarations
static int snd_aes67_new_pcm(struct aes67 *virtcard);

/* Destructor */
static int snd_aes67_free(struct aes67 *virtcard)
{
	snd_card_free(virtcard->card);
	kfree(virtcard);
	return 0;
}

/* Component Free */
static int snd_aes67_dev_free(struct snd_device *device)
{
	return snd_aes67_free(device->device_data);
}

/* Constructor */

static int snd_aes67_create(struct snd_card *card, struct aes67 **rvirtcard)
{
	struct aes67 *virtcard;
	int err;
	static const struct snd_device_ops ops = {
		.dev_free = snd_aes67_dev_free,
	};

	*rvirtcard = NULL;

	/* Setup Connection Handlers */

	/* allocate memory for virt card */
	virtcard = kzalloc(sizeof(*virtcard), GFP_KERNEL);
	if (virtcard == NULL)
		return -ENOMEM;

	virtcard->card = card;

	/* COnnect to things */

	/* Build Sound Device */
	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, virtcard, &ops);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create AES67 device\n");
		snd_aes67_free(virtcard);
		return err;
	}

	/* Add PCM */
	err = snd_aes67_new_pcm(virtcard);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create PCM for AES67 device\n");
		snd_aes67_free(virtcard);
		return err;
	}


	*rvirtcard = virtcard;
	return 0;
}

/* Probe method */
static int aes67_probe(struct platform_device *devptr)
{
	struct snd_card *card;
	struct aes67 *virtcard;
	int dev = devptr->id;
	int err;

	snd_printk(KERN_INFO "Attempting to create Soundcard for AES67\n");
	err = snd_devm_card_new(&devptr->dev, index[dev], id[dev], THIS_MODULE,
				sizeof(struct aes67), &card);
	if (err < 0) {
		snd_printk(KERN_ERR "Unable to create AES67 Soundcard\n");
		return err;
	}
	virtcard = card->private_data;

	snd_printk(KERN_INFO "Attempting to create AES67 Virtual Soundcard\n");
	err = snd_aes67_create(card, &virtcard);
	if (err < 0) {
		snd_printk(KERN_ERR "Unable to create AES67rtual Soundcard\n");
		goto error;
	}

	/* Setup Names */
	strcpy(card->driver, "AES67 VSC");
	strcpy(card->shortname, "AES67 Virtual Soundcard");
	strcpy(card->longname, "AES67 Virtual Soundcard - 8x8");

	snd_printk(KERN_INFO
		   "Attempting to Register AES67 Virtual Soundcard\n");
	err = snd_card_register(card);
	if (err < 0) {
		snd_printk(KERN_ERR
			   "Unable to Register AES67 Virtual Soundcard\n");
		goto error;
	}
	platform_set_drvdata(devptr, card);
	return 0;

error:
	snd_card_free(card);
	return err;
}

/* PCM Shenaniagians */
/* Playback definition */
static struct snd_pcm_hardware snd_aes67_playback_hw = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 32768,
	.period_bytes_min = 4096,
	.period_bytes_max = 32768,
	.periods_min = 1,
	.periods_max = 1024,
};

/* Capture definition */
static struct snd_pcm_hardware snd_aes67_capture_hw = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 32768,
	.period_bytes_min = 4096,
	.period_bytes_max = 32768,
	.periods_min = 1,
	.periods_max = 1024,
};

static int snd_aes67_playback_open(struct snd_pcm_substream *substream)
{
	struct aes67 *virtcard = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	runtime->hw = snd_aes67_playback_hw;
	return 0;
}

static int snd_aes67_playback_close(struct snd_pcm_substream *substream)
{
	struct aes67 *virtcard = snd_pcm_substream_chip(substream);
	return 0;
}

static int snd_aes67_capture_open(struct snd_pcm_substream *substream)
{
	struct aes67 *virtcard = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	runtime->hw = snd_aes67_capture_hw;
	return 0;
}

static int snd_aes67_capture_close(struct snd_pcm_substream *substream)
{
	struct aes67 *virtcard = snd_pcm_substream_chip(substream);
	return 0;
}

static int snd_aes67_pcm_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	/* the hardware-specific codes will be here */
	return 0;
}

/* hw_free callback */
static int snd_aes67_pcm_hw_free(struct snd_pcm_substream *substream)
{
	/* the hardware-specific codes will be here */
	return 0;
}

static int snd_aes67_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct aes67 *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	/* set up the hardware with the current configuration
         * for example...
         */
	return 0;
}

static int snd_aes67_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* do something to start the PCM engine */
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* do something to stop the PCM engine */
		break;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static snd_pcm_uframes_t aes67_get_hw_pointer(struct aes67 *virtcard)
{
	return 0;
}

static snd_pcm_uframes_t
snd_aes67_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct aes67 *chip = snd_pcm_substream_chip(substream);
	unsigned int current_frame;

	/* get the current hardware pointer */
	current_frame = aes67_get_hw_pointer(chip);
	return current_frame;
}

/* operators */
static struct snd_pcm_ops snd_aes67_playback_ops = {
	.open = snd_aes67_playback_open,
	.close = snd_aes67_playback_close,
	.hw_params = snd_aes67_pcm_hw_params,
	.hw_free = snd_aes67_pcm_hw_free,
	.prepare = snd_aes67_pcm_prepare,
	.trigger = snd_aes67_pcm_trigger,
	.pointer = snd_aes67_pcm_pointer,
};

/* operators */
static struct snd_pcm_ops snd_aes67_capture_ops = {
	.open = snd_aes67_capture_open,
	.close = snd_aes67_capture_close,
	.hw_params = snd_aes67_pcm_hw_params,
	.hw_free = snd_aes67_pcm_hw_free,
	.prepare = snd_aes67_pcm_prepare,
	.trigger = snd_aes67_pcm_trigger,
	.pointer = snd_aes67_pcm_pointer,
};

static int snd_aes67_new_pcm(struct aes67 *virtcard)
{
	struct snd_pcm *pcm;
	int err;

	snd_printk(KERN_INFO "Initializing PCM for Virtual Soundcard");
	err = snd_pcm_new(virtcard->card, CARD_NAME, 0, 1, 1, &pcm);
	if (err < 0)
		snd_printk(KERN_INFO "Failed initializing PCM for Virtual Soundcard");
		return err;

	snd_printk(KERN_INFO "Assigning PCM to Virtual Soundcard");
	pcm->private_data = virtcard;
	strcpy(pcm->name, CARD_NAME);
	virtcard->pcm = pcm;
	/* set operators */
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_aes67_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_aes67_capture_ops);
	/*Da buffers*/
	snd_pcm_set_managed_buffer_all(pcm, SNDRV_DMA_TYPE_DEV, NULL, 64 * 1024,
				       64 * 1024);
	return 0;
}

/* Power Methods */
static int snd_aes67_suspend(struct device *pdev)
{
	struct snd_card *card = dev_get_drvdata(pdev);

	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	return 0;
}

static int snd_aes67_resume(struct device *pdev)
{
	struct snd_card *card = dev_get_drvdata(pdev);

	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}
/* Power management operations. Most definitely need to revisit this */
static DEFINE_SIMPLE_DEV_PM_OPS(snd_aes67_pm, snd_aes67_suspend,
				snd_aes67_resume);

#define SND_AES67_DRIVER "snd_aes67"

static struct platform_driver snd_aes67_driver = {
	.probe = aes67_probe,
	.driver = {
		.name = SND_AES67_DRIVER,
		.pm = &snd_aes67_pm,
	},
};

/* Module init and exit functions */
static int __init alsa_card_aes67_init(void)
{
	int err;

	//Add driver to registry
	snd_printk(KERN_INFO "Attempting to register driver for AES67\n");
	err = platform_driver_register(&snd_aes67_driver);
	if (err < 0) {
		snd_printk(KERN_ERR "FAILED to register driver for AES67\n");
		return err;
	}

	//register a card in the kernel
	struct platform_device *device;
	device = platform_device_register_simple(SND_AES67_DRIVER, 0, NULL, 0);
	if (IS_ERR(device)) {
		snd_printk(KERN_ERR "Failed to register AES67 Device\n");
		return -ENODEV;
	}
	if (!platform_get_drvdata(device)) {
		snd_printk(KERN_ERR "No device data for AES67\n");
		platform_device_unregister(device);
		return -ENODEV;
	}
	devices[0] = device;

	return 0;
}

static void __exit alsa_card_aes67_exit(void)
{
	snd_printk(KERN_INFO "Attempting to unregister card for AES67\n");
	platform_device_unregister(devices[0]);
	snd_printk(KERN_INFO "Attempting to unregistered driver for AES67\n");
	platform_driver_unregister(&snd_aes67_driver);
}

module_init(alsa_card_aes67_init)
module_exit(alsa_card_aes67_exit)
