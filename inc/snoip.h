#ifndef MOD_SNOIP
#define MOD_SNOIP

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/initval.h>

/*
 * RTP stream
 *
 */

#define RTP_PAYLOAD_SIZE 1446

struct snoip_rtp_stream {
	bool empty;
	uint32_t sync_source;
	uint32_t size;
	atomic_long_t net_reader;
	atomic_long_t net_writer;
    atomic_long_t hw_reader;
    atomic_long_t hw_writer;
	uint32_t *packet_info;
	uint32_t *timestamp;
	uint32_t *csrc;
	uint8_t *data;
};


/* Definistion of AES67 Virtual SoundCard */
struct snd_aes67_vhw {
	/* ALSA Soundcard*/
	struct snd_card *card;
	/* PCM Device Soundcard*/
	struct snd_pcm *pcm;
	/* Linux Device */
	struct device *dev;
	/* Socket */

	/* Streams */
	struct aes67_rtp_stream *rx;
	struct aes67_rtp_stream *tx;
};


/* Definition of stream abstraction*/
struct aes67_rtp_stream {
	bool running;
	spinlock_t lock;
	struct work_struct work;
	struct socket *socket;
    struct snd_pcm_substream *pcm_substream;
    atomic_t *head;

    void (*original_data_ready)(struct sock *sk);
};

#endif
