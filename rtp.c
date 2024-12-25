#include <snoip.h>

static int snoip_rtp_stream_create(struct snoip_rtp_stream **stream,
				   size_t size)
{
	struct snoip_rtp_stream *strm;

	*stream = NULL;
	strm = kzalloc(sizeof(*strm), GFP_KERNEL);
	if (strm == NULL)
		return -ENOMEM;

	//Allocate arrays
	strm->csrc = kzalloc(size * sizeof(uint32_t), GFP_KERNEL);
	if (strm->csrc == NULL)
		return -ENOMEM;

	strm->timestamp = kzalloc(size * sizeof(uint32_t), GFP_KERNEL);
	if (strm->timestamp == NULL)
		return -ENOMEM;

	strm->packet_info = kzalloc(size * sizeof(uint32_t), GFP_KERNEL);
	if (strm->packet_info == NULL)
		return -ENOMEM;

	strm->data =
		kzalloc(size * RTP_PAYLOAD_SIZE * sizeof(uint8_t), GFP_KERNEL);
	if (strm->data == NULL)
		return -ENOMEM;

	*stream = strm;
	return 0;
}

static void snoip_rtp_steam_free(struct snoip_rtp_stream *stream)
{
	if (stream->data)
		kfree(stream->data);

	if (stream->csrc)
		kfree(stream->csrc);

	if (stream->timestamp)
		kfree(stream->timestamp);

	if (stream->packet_info)
		kfree(stream->packet_info);
}

static int snoip_rtp_stream_write(struct snoip_rtp_stream *stream,
				  unsigned char *packet_buf)
{
	if (packet_buf == NULL)
		return EINVAL;

	//construct packet info block from first 32 bits
	uint32_t packet_info = (packet_buf[0] << 24) | (packet_buf[1] << 16) |
			       (packet_buf[2] << 8) | packet_buf[3];

	uint16_t sequence_number = (packet_buf[2] << 8) | packet_buf[3];

	uint32_t timestamp = (packet_buf[4] << 24) | (packet_buf[5] << 16) |
			     (packet_buf[6] << 8) | packet_buf[7];

	uint32_t ssrc = (packet_buf[8] << 24) | (packet_buf[9] << 16) |
			(packet_buf[10] << 8) | packet_buf[11];
	stream->sync_source = ssrc;

	uint32_t csrc = (packet_buf[12] << 24) | (packet_buf[13] << 16) |
			(packet_buf[14] << 8) | packet_buf[15];

	int start_idx = (sequence_number % stream->size) * 1446;
	if (stream->empty) {
		stream->empty = false;
		stream->reader = start_idx;
	}

	stream->packet_info[start_idx] = packet_info;
	stream->timestamp[start_idx] = timestamp;
	stream->csrc[start_idx] = csrc;

	memcpy(stream->data + start_idx, packet_buf + 16, RTP_PAYLOAD_SIZE);

	//The idea is the writer will always be at the furtherest written packet.
	stream->writer = (stream->writer == 0 || stream->writer < start_idx) ?
				 start_idx :
				 stream->writer;

	return 0;
}
