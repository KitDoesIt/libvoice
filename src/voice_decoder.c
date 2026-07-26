/**
 * @file voice_decoder.c
 * @brief Decoder implementation: seq tracking, gap detection, stats, Opus decode.
 *
 * The decoder expects each packet to have a 2-byte uint16 sequence number
 * prepended by voice_encode().  It uses this to:
 *  - Detect packet loss (gaps) and transparently insert PLC frames
 *  - Detect out-of-order / late packets
 *  - Track smoothed loss percentage and other network metrics
 *  - Produce a VoiceNetReport with recommended encoder adjustments
 */

#include <stdlib.h>
#include <string.h>
#include <opus.h>
#include <rnnoise.h>

#include "voice_codec.h"

/* Smoothing factor for EWMA loss rate: 0.1 = ~2s window at 10ms frames */
#define LOSS_EWMA_ALPHA  0.1f

/* --- Helper: uint16 sequence comparison with wraparound --- */
static int seq_gt(uint16_t a, uint16_t b)
{
    return (uint16_t)(a - b) < 32768;
}

struct VoiceDecoder {
    OpusDecoder     *opus_dec;
    DenoiseState    *rnnoise_state;
    int              denoise_enabled;

    /* Sequence tracking */
    uint16_t         expected_seq;
    int              first_packet;

    /* Saved "future" packet — arrived before its turn */
    uint8_t          saved_data[VOICE_CODEC_MAX_PACKET];
    int              saved_len;
    int              has_saved;

    /* Statistics */
    int              total_received;
    int              total_lost;
    int              consecutive_lost;
    int              out_of_order_count;
    float            loss_ewma;          /* smoothed loss rate 0-100 */
};

VoiceDecoder *voice_decoder_create(int denoise_enabled)
{
    int err;
    VoiceDecoder *dec = calloc(1, sizeof(*dec));
    if (!dec) return NULL;

    dec->denoise_enabled = denoise_enabled;
    dec->first_packet    = 1;

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

/* ------------------------------------------------------------------ */
/*  Internal: decode one opus frame, optionally with FEC              */
/* ------------------------------------------------------------------ */
static int decode_frame(VoiceDecoder *dec, const uint8_t *data, int len,
                        float *pcm_out)
{
    int nb = opus_decode_float(
        dec->opus_dec,
        len > 0 ? data : NULL,
        len > 0 ? len : 0,
        pcm_out,
        VOICE_CODEC_FRAME_SIZE,
        0);
    return nb;
}

/* ------------------------------------------------------------------ */
/*  Internal: perform PLC for the current expected frame              */
/* ------------------------------------------------------------------ */
static int do_plc(VoiceDecoder *dec, float *pcm_out)
{
    dec->loss_ewma = (1.0f - LOSS_EWMA_ALPHA) * dec->loss_ewma
                   + LOSS_EWMA_ALPHA * 100.0f;
    dec->total_lost++;
    dec->consecutive_lost++;
    dec->expected_seq++;
    return decode_frame(dec, NULL, 0, pcm_out);
}

/* ------------------------------------------------------------------ */
/*  voice_decode                                                      */
/* ------------------------------------------------------------------ */
int voice_decode(
    VoiceDecoder       *dec,
    const uint8_t      *data_in,
    int                 data_in_len,
    float              *pcm_out)
{
    if (!dec || !pcm_out)
        return -1;

    /* --- Discard path (late/duplicate packet) --- */
    if (data_in_len < 0) {
        dec->out_of_order_count++;
        return do_plc(dec, pcm_out);
    }

    /* --- PLC path (explicit request for concealment) --- */
    if (data_in_len == 0) {
        return do_plc(dec, pcm_out);
    }

    /* --- Normal path: packet with seq header --- */
    if (data_in_len < 3)
        return -2;  /* too short for seq header + data */

    uint16_t seq = data_in[0] | ((uint16_t)data_in[1] << 8);
    const uint8_t *opus_data = data_in + 2;
    int opus_len = data_in_len - 2;

    /* First packet ever → initialise sequence tracker */
    if (dec->first_packet) {
        dec->expected_seq = seq;
        dec->first_packet = 0;
    }

    /* --- Out-of-order (late or duplicate) --- */
    if (seq_gt(dec->expected_seq, seq)) {
        dec->out_of_order_count++;
        return do_plc(dec, pcm_out);
    }

    /* --- Gap detected: one or more packets were lost --- */
    int gap = (int)(seq - dec->expected_seq);
    if (gap > 0) {
        dec->total_lost += gap;
        dec->consecutive_lost += gap;
        for (int i = 0; i < gap; i++) {
            dec->loss_ewma = (1.0f - LOSS_EWMA_ALPHA) * dec->loss_ewma
                           + LOSS_EWMA_ALPHA * 100.0f;
            float dummy[VOICE_CODEC_FRAME_SIZE];
            decode_frame(dec, NULL, 0, dummy);
            dec->expected_seq++;
        }
    }

    /* --- In-sequence packet → normal decode --- */
    dec->consecutive_lost = 0;
    dec->total_received++;
    dec->expected_seq = seq + 1;

    /* Update EWMA with 0% loss for this frame */
    dec->loss_ewma = (1.0f - LOSS_EWMA_ALPHA) * dec->loss_ewma;

    int nb = decode_frame(dec, opus_data, opus_len, pcm_out);
    if (nb < 0) return nb;

    /* --- Optional post-decode denoise (unusual) --- */
    if (dec->denoise_enabled && dec->rnnoise_state && nb > 0) {
        float denoised[VOICE_CODEC_FRAME_SIZE];
        rnnoise_process_frame(dec->rnnoise_state, denoised, pcm_out);
        memcpy(pcm_out, denoised, nb * sizeof(float));
    }

    return nb;
}

/* ------------------------------------------------------------------ */
/*  Network report                                                    */
/* ------------------------------------------------------------------ */
void voice_decoder_get_report(VoiceDecoder *dec, VoiceNetReport *report)
{
    if (!dec || !report) return;
    memset(report, 0, sizeof(*report));

    report->loss_percent       = (int)(dec->loss_ewma + 0.5f);
    report->consecutive_lost   = dec->consecutive_lost;
    report->out_of_order_count = dec->out_of_order_count;
    report->total_received     = dec->total_received;
    report->total_lost         = dec->total_lost;

    /* --- Recommend FEC ---
     *   0% loss  → FEC off (save bitrate)
     *   1-5%     → FEC 1 (light protection)
     *   >5%      → FEC 2 (heavier, SILK for music too) */
    if (report->loss_percent < 1)
        report->fec_recommend = 0;
    else if (report->loss_percent <= 5)
        report->fec_recommend = 1;
    else
        report->fec_recommend = 2;

    /* --- Recommend bitrate ---
     *   No change unless loss is severe (>15%) → reduce bitrate
     *   to increase resilience, or very clean → could go higher. */
    if (report->loss_percent > 15)
        report->bitrate_recommend = 16000;   /* drop to 16 kbps */
    else
        report->bitrate_recommend = -1;      /* no change */

    /* Reset per-report counters */
    dec->out_of_order_count = 0;
}
