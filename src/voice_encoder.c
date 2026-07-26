/**
 * @file voice_encoder.c
 * @brief Encoder implementation: RNNoise denoise → Opus encode.
 */

#include <stdlib.h>
#include <string.h>
#include <opus.h>
#include <rnnoise.h>

#include "voice_codec.h"

struct VoiceEncoder {
    OpusEncoder     *opus_enc;
    DenoiseState    *rnnoise_state;
    int              denoise_enabled;
};

VoiceEncoder *voice_encoder_create(int bitrate, int denoise_enabled)
{
    int err;
    VoiceEncoder *enc = calloc(1, sizeof(*enc));
    if (!enc) return NULL;

    enc->denoise_enabled = denoise_enabled;

    /* Create Opus encoder: 48 kHz, mono, VOIP mode (optimised for voice) */
    enc->opus_enc = opus_encoder_create(
        VOICE_CODEC_SAMPLE_RATE,
        1,                          /* mono */
        OPUS_APPLICATION_VOIP,
        &err);
    if (err != OPUS_OK) {
        free(enc);
        return NULL;
    }

    /* Configure Opus for voice */
    if (bitrate > 0) {
        opus_encoder_ctl(enc->opus_enc, OPUS_SET_BITRATE(bitrate));
    }
    /* 10 ms frame size is the default; enforce it explicitly */
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
    /* Voice-optimised defaults */
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_PACKET_LOSS_PERC(10));
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_DTX(1));
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_COMPLEXITY(5));

    /* Create RNNoise denoiser if enabled */
    if (denoise_enabled) {
        enc->rnnoise_state = rnnoise_create(NULL);  /* NULL = default model */
        /* Non-fatal: if RNNoise fails, still return the encoder but
         * denoising will be silently skipped in voice_encode(). */
    }

    return enc;
}

void voice_encoder_destroy(VoiceEncoder *enc)
{
    if (!enc) return;
    if (enc->opus_enc)       opus_encoder_destroy(enc->opus_enc);
    if (enc->rnnoise_state)  rnnoise_destroy(enc->rnnoise_state);
    free(enc);
}

int voice_encode(
    VoiceEncoder  *enc,
    const float   *pcm_in,
    uint8_t       *data_out,
    int            data_out_capacity)
{
    if (!enc || !pcm_in || !data_out || data_out_capacity <= 0)
        return -1;

    const float *src = pcm_in;
    float denoised_buf[VOICE_CODEC_FRAME_SIZE];

    /* --- Denoise stage (optional) --- */
    if (enc->denoise_enabled && enc->rnnoise_state) {
        rnnoise_process_frame(enc->rnnoise_state, denoised_buf, pcm_in);
        src = denoised_buf;
    }

    /* --- Opus encode --- */
    int nb = opus_encode_float(
        enc->opus_enc,
        src,
        VOICE_CODEC_FRAME_SIZE,
        data_out,
        data_out_capacity);

    return nb;  /* bytes written or negative error */
}

int voice_encode_flush(VoiceEncoder *enc, uint8_t *data_out, int data_out_capacity)
{
    (void)enc;
    (void)data_out;
    (void)data_out_capacity;
    /* Opus in 10 ms VOIP mode does not buffer across encode calls.
     * (DTX frames are emitted inline in opus_encode_float.)
     * So there is nothing extra to flush — return 0. */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Runtime configuration                                             */
/* ------------------------------------------------------------------ */

void voice_encoder_set_bitrate(VoiceEncoder *enc, int bitrate)
{
    if (!enc || !enc->opus_enc) return;
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_BITRATE(bitrate));
}

void voice_encoder_set_packet_loss(VoiceEncoder *enc, int loss_pct)
{
    if (!enc || !enc->opus_enc) return;
    if (loss_pct < 0)  loss_pct = 0;
    if (loss_pct > 100) loss_pct = 100;
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_PACKET_LOSS_PERC(loss_pct));
}

void voice_encoder_set_fec(VoiceEncoder *enc, int enable)
{
    if (!enc || !enc->opus_enc) return;
    if (enable < 0) enable = 0;
    if (enable > 2) enable = 2;
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_INBAND_FEC(enable));
}

void voice_encoder_set_dtx(VoiceEncoder *enc, int enable)
{
    if (!enc || !enc->opus_enc) return;
    opus_encoder_ctl(enc->opus_enc, OPUS_SET_DTX(enable ? 1 : 0));
}
