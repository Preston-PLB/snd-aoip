// SPDX-License-Identifier: GPL-1.0-or-later
/*
 *  AES67 soundcard
 *
 *  */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <net/net_namespace.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/initval.h>

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;
static int pcm_devs[SNDRV_CARDS] = { [0 ...(SNDRV_CARDS - 1)] = 1 };
static int pcm_substreams[SNDRV_CARDS] = { [0 ...(SNDRV_CARDS - 1)] = 8 };

/* work for the network streams */
static struct workqueue_struct *io_workqueue;

static struct platform_device *devices[SNDRV_CARDS];

#define CARD_NAME "AES67 Virtual Soundcard"

#define AES67_NET_SUCCESS 0

#define AES67_STREAM_RX 0
#define AES67_STREAM_TX 1

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

MODULE_AUTHOR("Preston Baxter <preston@preston-baxter.com>");
MODULE_DESCRIPTION("AES67 Virtual Soundcard");
MODULE_LICENSE("GPL");

/* Definition of stream abstraction*/
struct aes67_stream {
	bool running;
	struct socket *socket;
	spinlock_t lock;
	struct work_struct work;
};

/* Definistion of AES67 Virtual SoundCard */
struct aes67 {
	/* ALSA Soundcard*/
	struct snd_card *card;
	/* PCM Device Soundcard*/
	struct snd_pcm *pcm;
	/* Linux Device */
	struct device *dev;
	/* Socket */

	/* Streams */
	struct aes67_stream *rx;
	struct aes67_stream *tx;
};

/* Forward declarations */
static int snd_aes67_new_pcm(struct aes67 *virtcard);

static int work_start(void);
static void work_stop(void);

static void aes67_rx_net(struct work_struct *rwork);

static void aes67_stream_free(struct aes67_stream *stream);
static int aes67_stream_create(struct aes67_stream **stream, int direction);

/* Destructor */
static int snd_aes67_free(struct aes67 *virtcard)
{
	/* free card */
	snd_printk(KERN_INFO "Freeing Soundcard\n");
	snd_card_free(virtcard->card);

	/* free streams */
	if (virtcard->rx) {
		snd_printk(KERN_INFO "Freeing RX stream\n");
		aes67_stream_free(virtcard->rx);
	}

	if (virtcard->tx) {
		snd_printk(KERN_INFO "Freeing TX stream\n");
		aes67_stream_free(virtcard->tx);
	}

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
	/* allocate memory for virt card */
	virtcard = kzalloc(sizeof(*virtcard), GFP_KERNEL);
	if (virtcard == NULL)
		return -ENOMEM;

	virtcard->card = card;

	/* Build Sound Device */
	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, virtcard, &ops);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create AES67 device\n");
		goto init_fail;
	}

	/* Create Streams */
	err = aes67_stream_create(&virtcard->rx, AES67_STREAM_RX);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create AES67 RX stream\n");
		goto init_fail;
	}
	err = aes67_stream_create(&virtcard->tx, AES67_STREAM_TX);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create AES67 TX stream\n");
		goto init_fail;
	}

	/* Add PCM */
	err = snd_aes67_new_pcm(virtcard);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create PCM for AES67 device\n");
		goto init_fail;
	}
	goto success;

init_fail:
	snd_aes67_free(virtcard);
	return err;

success:
	snd_printk(KERN_INFO "Successfully created AES67\n");
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
	virtcard->dev = &devptr->dev; //I don't know if this is safe

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
/* Work queue Management */
static void work_stop(void)
{
	if (io_workqueue) {
		destroy_workqueue(io_workqueue);
		io_workqueue = NULL;
	}
}

static int work_start(void)
{
	io_workqueue = alloc_workqueue(
		"aes67_io", WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_UNBOUND, 0);

	if (!io_workqueue) {
		snd_printk(KERN_ERR "Failed to start io workqueue struct\n");
		return -ENOMEM;
	}
	return 0;
}

/* Network functions */

static void aes67_rx_net(struct work_struct *work)
{
	struct aes67_stream *stream =
		container_of(work, struct aes67_stream, work);
	struct msghdr msg = {};
	size_t recv_buf_size = 4096;
	ssize_t msglen;
	unsigned char *recv_buf;

	/* Loop Receive */
	recv_buf = kzalloc(recv_buf_size, GFP_KERNEL);
	if (!recv_buf) {
		snd_printk(KERN_ERR
			   "Failed to allocate network receive buffer\n");
		return;
	}

	msg.msg_flags = MSG_WAITFORONE;

	snd_printk(KERN_INFO "Starting network receive loop\n");
	for (;;) {
		if (!stream->running)
			break;

		struct kvec iv;
		iv.iov_base = recv_buf;
		iv.iov_len = recv_buf_size;

		msglen = kernel_recvmsg(stream->socket, &msg, &iv, 1,
					iv.iov_len, msg.msg_flags);

		if (msglen == -EAGAIN) {
			snd_printk(
				KERN_WARNING
				"Failed to receive message. With EAGAIN, Sleeping and trying again\n");
			msleep(1000);
			continue;
		}

		if (msglen < 0) {
			snd_printk(KERN_ERR "error receiving packet: %zd\n",
				   msglen);
			break;
		}

		if (msglen > 0) {
			snd_printk(KERN_INFO "Received Buffer: %s\n", recv_buf);
			continue;
		}
	}
	/* Handle stream end */
	snd_printk(KERN_INFO "Finished Receiving work queue\n");
	kfree(recv_buf);

	return;
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
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aes67 *chip = snd_pcm_substream_chip(substream);

	/* Start receive loop */
	spin_lock(&chip->rx->lock);
	if (chip->rx && !chip->rx->running) {
		snd_printk(KERN_INFO "Starting RX work queue\n");
		queue_work(io_workqueue, &chip->rx->work);
		chip->rx->running = true;
	}
	spin_unlock(&chip->rx->lock);

	/* Start transmit loop */
	spin_lock(&chip->tx->lock);
	if (chip->tx && !chip->tx->running) {
		snd_printk(KERN_INFO "Starting RX work queue\n");
		queue_work(io_workqueue, &chip->tx->work);
		chip->tx->running = true;
	}
	spin_unlock(&chip->tx->lock);

	runtime->hw = snd_aes67_playback_hw;
	return 0;
}

static int snd_aes67_playback_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int snd_aes67_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	runtime->hw = snd_aes67_capture_hw;
	return 0;
}

static int snd_aes67_capture_close(struct snd_pcm_substream *substream)
{
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
	/* set up the hardware with the current configuration
         * for example...
         */
	return 0;
}

static int snd_aes67_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
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
	if (err < 0) {
		snd_printk(KERN_INFO
			   "Failed initializing PCM for Virtual Soundcard");
		return err;
	}

	snd_printk(KERN_INFO "Assigning PCM to Virtual Soundcard");
	pcm->private_data = virtcard;
	strcpy(pcm->name, CARD_NAME);
	virtcard->pcm = pcm;
	/* set operators */
	snd_printk(KERN_INFO "Settig PCM Playback ops");
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_aes67_playback_ops);
	snd_printk(KERN_INFO "Settig PCM Capture ops");
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_aes67_capture_ops);
	/*Da buffers*/
	snd_printk(KERN_INFO "Settig PCM managed buffer");
	snd_pcm_set_managed_buffer_all(pcm, SNDRV_DEV_LOWLEVEL, virtcard->dev,
				       32 * 1024, 32 * 1024);
	return 0;
}

static void aes67_stream_free(struct aes67_stream *stream)
{
	stream->running = false;
	if (stream->socket && stream->socket->ops) {
		stream->socket->ops->release(stream->socket);
	}

	kfree(stream);
}

static int aes67_stream_create(struct aes67_stream **stream, int direction)
{
	struct aes67_stream *strm;
	int err;

	strm = kzalloc(sizeof(*strm), GFP_KERNEL);
	if (!strm)
		return -ENOMEM;

	/* create socket */
	err = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP,
			       &strm->socket);
	if (err < 0) {
		snd_printk(KERN_ERR "Failed to create socket for stream\n");
		return err;
	}

	struct sockaddr_in addr = { .sin_family = AF_INET,
				    .sin_port = htons(9375),
				    .sin_addr = { htonl(INADDR_LOOPBACK) } };

	strm->socket->ops->bind(strm->socket, (struct sockaddr *)&addr,
				sizeof(addr));
	if (err < 0) {
		snd_printk(KERN_ERR
			   "Failed to bind socket for virtual soundcard\n");
		return err;
	}

	/* Spin lock for receiving */
	spin_lock_init(&strm->lock);
	/* Init work for card */
	switch (direction) {
	case AES67_STREAM_RX:
		INIT_WORK(&strm->work, aes67_rx_net);
		break;
	default:
		snd_printk(KERN_WARNING "Unimplemented Direction\n");
	}

	*stream = strm;
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

	/* Start work queue */
	err = work_start();
	if (err < 0) {
		snd_printk(KERN_ERR "FAILED to start workqueue for AES67\n");
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
	snd_printk(KERN_INFO "Attempting to stop workqueue for AES67\n");
	work_stop();
}

module_init(alsa_card_aes67_init) module_exit(alsa_card_aes67_exit)
