#ifndef MOD_SNOIP
#define MOD_SNOIP

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

//allocate new rtp stream
int snoip_rtp_stream_create(struct snoip_rtp_stream **stream, size_t size);
//free rtp stream
void snoip_rtp_steam_free(struct snoip_rtp_stream *stream);

//write rtp packet to stream
int snoip_rtp_stream_write(struct snoip_rtp_stream *stream,
				  const uint8_t *packet_buf, size_t packet_len);
//copy rtp stream buffer to dma region
int snoip_rtp_stream_copy_dma(struct snoip_rtp_stream *stream,
				     struct snd_pcm_runtime *runtime, uint32_t bytes);

#endif
