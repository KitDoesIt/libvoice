/**
 * @file voice_decoder.c
 * @brief Decoder implementation: Opus decode → optional RNNoise denoise.
 */

#include <stdlib.h>
#include <string.h>
#include <opus.h>
#include <rnnoise.h>

#include "voice_codec.h"

struct VoiceDecoder {
    OpusDecoder     *opus_dec;
    DenoiseState    *rnnoise_state;
    int              denoise_enabled;
};

VoiceDecoder *voice_decoder_create(int denoise_enabled)
{
    int err;
    VoiceDecoder *dec = calloc(1, sizeof(*dec));
    if (!dec) return NULL;

    dec->denoise_enabled = denoise_enabled;

    dec->opus_dec = opus_decoder_create(
        VOICE_CODEC_SAMPLE_RATE,
        1,                          /* mono */
        &err);
    if (err != OPUS_OK) {
        free(dec);
        return NULL;
    }

    if (denoise_enabled) {
        dec->rnnoise_state = rnnoise_create(NULL);
    }

    return dec;
}

void voice_decoder_destroy(VoiceDecoder *dec)
{
    if (!dec) return;
    if (dec->opus_dec)       opus_decoder_destroy(dec->opus_dec);
    if (dec->rnnoise_state)  rnnoise_destroy(dec->rnnoise_state);
    free(dec);
}

int voice_decode(
    VoiceDecoder       *dec,
    const uint8_t      *data_in,
    int                 data_in_len,
    float              *pcm_out)
{
    if (!dec || !pcm_out)
        return -1;

    /* --- Opus decode ---
     * When data_in_len == 0, opus_decode_float performs packet loss
     * concealment (PLC) and returns VOICE_CODEC_FRAME_SIZE. */
    int nb_samples = opus_decode_float(
        dec->opus_dec,
        data_in_len ? data_in : NULL,
        data_in_len,
        pcm_out,
        VOICE_CODEC_FRAME_SIZE,
        0);                         /* decode_fec = 0 (no FEC) */

    if (nb_samples < 0)
        return nb_samples;          /* error code */

    /* --- Optional denoise after decode (unusual but supported) --- */
    if (dec->denoise_enabled && dec->rnnoise_state && nb_samples > 0) {
        float denoised[VOICE_CODEC_FRAME_SIZE];
        rnnoise_process_frame(dec->rnnoise_state, denoised, pcm_out);
        memcpy(pcm_out, denoised, nb_samples * sizeof(float));
    }

    return nb_samples;
}
