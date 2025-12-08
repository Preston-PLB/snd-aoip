#include <snoip.h>

int snoip_rtp_stream_create(struct snoip_rtp_stream **stream, size_t size)
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

	strm->empty = true;
    strm->size = size;

	*stream = strm;
	return 0;
}

void snoip_rtp_steam_free(struct snoip_rtp_stream *stream)
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

// The minimum fixed header is 12 bytes
typedef struct {
    uint8_t vpxcc;      // Byte 0: V(2), P(1), X(1), CC(4)
    uint8_t mpt;        // Byte 1: M(1), PT(7)
    uint16_t sequence_number; // Bytes 2-3 (Network Byte Order)
    uint32_t timestamp;       // Bytes 4-7 (Network Byte Order)
    uint32_t ssrc;            // Bytes 8-11 (Network Byte Order)
} rtp_fixed_header_t;

int snoip_rtp_stream_write(struct snoip_rtp_stream *stream,
				  const uint8_t *packet_buf, size_t packet_len)
{
    if (stream == NULL){
        printk(KERN_ERR "snoip: Received NULL stream pointer in snoip_rtp_stream_write.\n");
		return EINVAL;
    }
	if (packet_buf == NULL)
		return EINVAL;

    const rtp_fixed_header_t *header = (const rtp_fixed_header_t *)packet_buf;

    // --- Extracting the 1st byte (V, P, X, CC) ---
    //uint8_t v = (header->vpxcc >> 6) & 0x03; // V: Bits 6-7 (2 bits)
    uint8_t padding_flag = (header->vpxcc >> 5) & 0x01; // P: Bit 5 (1 bit)
    //uint8_t x = (header->vpxcc >> 4) & 0x01; // X: Bit 4 (1 bit)
    //uint8_t cc = header->vpxcc & 0x0F;       // CC: Bits 0-3 (4 bits)

	int start_idx = header->sequence_number % stream->size;
	if (stream->empty) {
		stream->empty = false;
		stream->reader = start_idx;
	}

	stream->packet_info[start_idx] = header->vpxcc;
	stream->timestamp[start_idx] = ntohl(header->timestamp);

    //TODO: Implement multiple CSRC
    size_t offset = 12;
    const uint32_t *csrc_ptr = (const uint32_t *)(packet_buf + offset);
	stream->csrc[start_idx] = csrc_ptr[0];

    if (packet_len > offset) {
        size_t payload_len = packet_len - offset;

        if (padding_flag == 1) {
            uint8_t padding_bytes = packet_buf[packet_len - 1];
            if (payload_len >= padding_bytes) {
                payload_len -= padding_bytes;
            } else {
                printk(KERN_ERR "Invalid padding count: %d exceeds payload size.\n", padding_bytes);
            }
        }
        memcpy(stream->data + (start_idx * RTP_PAYLOAD_SIZE), packet_buf + offset, payload_len);
    }

	return 0;
}

int snoip_rtp_stream_copy_dma(struct snoip_rtp_stream *stream,
			      struct snd_pcm_runtime *runtime, uint32_t bytes)
{
	char *dst = runtime->dma_area;
	char *src = stream->data;

	unsigned int stream_bytes = stream->size * RTP_PAYLOAD_SIZE;
	uint32_t b = bytes;

	unsigned int src_offset = stream->reader * RTP_PAYLOAD_SIZE;
	unsigned int dst_offset = stream->writer;

	for (;;) {
		uint32_t size = bytes;
		if (size + dst_offset > runtime->dma_bytes)
			size = runtime->dma_bytes - dst_offset;
		if (size + src_offset > stream->size * RTP_PAYLOAD_SIZE)
			size = runtime->dma_bytes - src_offset;
		else
			memcpy(dst + dst_offset, src + src_offset, size);

		bytes -= size;
		if (bytes == 0)
			break;
		src_offset = (src_offset + size) % stream_bytes;
		dst_offset = (dst_offset + size) % runtime->dma_bytes;
	}

	stream->reader = (src_offset + b) % stream_bytes;
	stream->writer = (dst_offset + b) % runtime->dma_bytes;

	return 0;
}
