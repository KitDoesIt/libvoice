/**
 * @file voice_codec.h
 * @brief Voice compression with automatic noise cancellation.
 *
 * This library combines RNNoise (neural-network noise suppression) with
 * Opus (audio compression) into a single, easy-to-use API designed
 * primarily for real-time voice transmission.
 *
 * Audio format: 48 kHz, mono, 32-bit float PCM, 10 ms frames (480 samples).
 *
 * Typical pipeline:
 *   Encode:  float PCM → [RNNoise denoise] → Opus compress → bytes
 *   Decode:  bytes → Opus decompress → float PCM
 *
 * Both encoder and decoder are single-threaded; for multi-stream use,
 * create separate instances.
 */

#ifndef VOICE_CODEC_H
#define VOICE_CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Visibility / export macros                                       */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef VOICE_CODEC_BUILD
    #define VOICE_CODEC_API __declspec(dllexport)
  #else
    #define VOICE_CODEC_API __declspec(dllimport)
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define VOICE_CODEC_API __attribute__((visibility("default")))
  #else
    #define VOICE_CODEC_API
  #endif
#endif

/* ------------------------------------------------------------------ */
/*  Opaque types                                                      */
/* ------------------------------------------------------------------ */

/** Opaque encoder handle. Created by voice_encoder_create(). */
typedef struct VoiceEncoder VoiceEncoder;

/** Opaque decoder handle. Created by voice_decoder_create(). */
typedef struct VoiceDecoder VoiceDecoder;

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

/** Sample rate used by this codec (always 48 kHz). */
#define VOICE_CODEC_SAMPLE_RATE  48000

/** Number of samples per frame (10 ms @ 48 kHz). */
#define VOICE_CODEC_FRAME_SIZE   480

/** Maximum size of an Opus-encoded frame (worst case).
 *  Includes 2-byte sequence number header. */
#define VOICE_CODEC_MAX_PACKET   4002

/* ------------------------------------------------------------------ */
/*  Network report (decoder → encoder feedback)                       */
/* ------------------------------------------------------------------ */

/**
 * Network quality report produced by the decoder.
 *
 * Call voice_decoder_get_report() periodically and, if desired,
 * relay the recommended settings back to the encoder via
 * voice_encoder_set_packet_loss() / voice_encoder_set_fec() /
 * voice_encoder_set_bitrate().
 */
typedef struct {
    /** Smoothed packet loss percentage (0–100). */
    int   loss_percent;
    /** Recommended FEC setting (0, 1, or 2). */
    int   fec_recommend;
    /** Recommended bitrate, or -1 for no change. */
    int   bitrate_recommend;
    /** Consecutive packets lost in the most recent burst.
     *  0 = no loss currently happening. */
    int   consecutive_lost;
    /** Count of out-of-order (late/duplicate) packets since last report. */
    int   out_of_order_count;
    /** Total packets received (wrapping). */
    int   total_received;
    /** Total packets lost (wrapping). */
    int   total_lost;
} VoiceNetReport;

/* ------------------------------------------------------------------ */
/*  Encoder API                                                       */
/* ------------------------------------------------------------------ */

/**
 * Create a new encoder.
 *
 * @param  bitrate           Target bitrate in bits per second.
 *                           Suggested range: 6000–128000 (voice).
 *                           Pass 0 for Opus default (auto).
 * @param  denoise_enabled   Non-zero to enable RNNoise noise suppression
 *                           before encoding.
 * @return                   Encoder handle, or NULL on allocation failure.
 */
VOICE_CODEC_API VoiceEncoder *voice_encoder_create(int bitrate, int denoise_enabled);

/**
 * Destroy an encoder and free all associated resources.
 */
VOICE_CODEC_API void voice_encoder_destroy(VoiceEncoder *enc);

/**
 * Encode one frame of PCM audio.
 *
 * The input is 480 float samples (mono, 48 kHz, range approx [-1, 1]).
 * If denoising is enabled, the input is first passed through RNNoise.
 *
 * The output is a 2-byte sequence number followed by the Opus packet.
 * Total size = 2 + opus_bytes.  The sequence number increments by 1
 * each frame (uint16, wraps at 65535).
 *
 * @param  enc               Encoder handle.
 * @param  pcm_in            Input PCM samples (480 floats).
 * @param  data_out          Output buffer.  Must hold at least
 *                           VOICE_CODEC_MAX_PACKET (4002) bytes.
 * @param  data_out_capacity Size of @p data_out in bytes.
 * @return Total number of bytes written (2 + opus bytes),
 *         or a negative value on error.
 */
VOICE_CODEC_API int voice_encode(
    VoiceEncoder  *enc,
    const float   *pcm_in,
    uint8_t       *data_out,
    int            data_out_capacity);

/**
 * Flush any buffered audio and return a final packet (if any).
 *
 * In DTX mode the encoder may delay output. This forces emission.
 * After calling flush, the encoder state is reset and can be reused
 * for a new stream.
 *
 * @return Number of bytes written, or 0 if nothing to flush,
 *         or negative on error.
 */
VOICE_CODEC_API int voice_encode_flush(VoiceEncoder *enc, uint8_t *data_out, int data_out_capacity);

/**
 * Set the target bitrate at runtime (bits per second).
 *
 * Can be called between encode calls to adapt to network conditions.
 *
 * @param enc     Encoder handle.
 * @param bitrate Bitrate in bps (500–512000), or 0 for Opus default.
 */
VOICE_CODEC_API void voice_encoder_set_bitrate(VoiceEncoder *enc, int bitrate);

/**
 * Set the expected packet loss percentage.
 *
 * Higher values make the encoder more resilient to loss at the cost
 * of slightly reduced quality when no loss occurs.
 *
 * @param enc        Encoder handle.
 * @param loss_pct   0–100 (default 0).
 */
VOICE_CODEC_API void voice_encoder_set_packet_loss(VoiceEncoder *enc, int loss_pct);

/**
 * Enable or disable in-band forward error correction (FEC).
 *
 * When enabled, each packet contains a low-bitrate copy of the
 * previous frame. At 1=FEC enabled, at 2=FEC enabled with auto
 * switch to SILK even for music.
 *
 * @param enc      Encoder handle.
 * @param enable   0=off, 1=on (SILK auto), 2=on (SILK for music too).
 */
VOICE_CODEC_API void voice_encoder_set_fec(VoiceEncoder *enc, int enable);

/**
 * Enable or disable discontinuous transmission (DTX).
 *
 * When DTX is on, the encoder emits tiny comfort-noise packets
 * during silence instead of encoding background noise at full bitrate.
 *
 * @param enc      Encoder handle.
 * @param enable   0=off, 1=on.
 */
VOICE_CODEC_API void voice_encoder_set_dtx(VoiceEncoder *enc, int enable);

/* ------------------------------------------------------------------ */
/*  Decoder API                                                       */
/* ------------------------------------------------------------------ */

/**
 * Create a new decoder.
 *
 * @param  denoise_enabled   Non-zero to apply RNNoise after decoding.
 *                           Typically 0 (noise is removed on encode side).
 * @return                   Decoder handle, or NULL on allocation failure.
 */
VOICE_CODEC_API VoiceDecoder *voice_decoder_create(int denoise_enabled);

/**
 * Destroy a decoder and free all associated resources.
 */
VOICE_CODEC_API void voice_decoder_destroy(VoiceDecoder *dec);

/**
 * Decode one frame from a compressed packet.
 *
 * The input must include the 2-byte sequence number header prepended
 * by voice_encode().  The decoder uses it to detect gaps and track
 * network statistics.
 *
 * If the sequence number reveals a gap (lost packets), the decoder
 * automatically performs packet loss concealment for the missing
 * frames internally and returns the decoded audio for the current
 * packet.  One call = one frame = 480 samples.
 *
 * @param  dec               Decoder handle.
 * @param  data_in           Compressed packet bytes (with 2-byte seq header).
 * @param  data_in_len       Number of bytes in @p data_in.
 *                           Pass 0 to request packet loss concealment (PLC)
 *                           for the next expected frame.
 *                           Pass -1 to discard (late/duplicate packet).
 * @param  pcm_out           Output PCM buffer (must hold at least 480 floats).
 * @return Number of samples decoded (typically 480),
 *         or a negative value on error.
 */
VOICE_CODEC_API int voice_decode(
    VoiceDecoder       *dec,
    const uint8_t      *data_in,
    int                 data_in_len,
    float              *pcm_out);

/**
 * Get current network quality report from the decoder.
 *
 * Call this periodically (e.g. every 500ms) and relay the
 * recommendations back to the encoder if desired.
 *
 * @param  dec     Decoder handle.
 * @param  report  Output structure filled with current metrics.
 */
VOICE_CODEC_API void voice_decoder_get_report(VoiceDecoder *dec, VoiceNetReport *report);

/* ------------------------------------------------------------------ */
/*  Utility / info                                                    */
/* ------------------------------------------------------------------ */

/** Return the frame size in samples (always 480). */
VOICE_CODEC_API int voice_get_frame_size(void);

/** Return the sample rate in Hz (always 48000). */
VOICE_CODEC_API int voice_get_sample_rate(void);

/** Return a human-readable version string. */
VOICE_CODEC_API const char *voice_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_CODEC_H */
