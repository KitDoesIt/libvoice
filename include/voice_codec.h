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

/** Maximum size of an Opus-encoded frame (worst case). */
#define VOICE_CODEC_MAX_PACKET   4000

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
 * The result is an Opus packet written to @p data_out.
 *
 * @param  enc               Encoder handle.
 * @param  pcm_in            Input PCM samples (480 floats).
 * @param  data_out          Output buffer for compressed packet.
 * @param  data_out_capacity Size of @p data_out in bytes (max bytes to write).
 *                           Recommend VOICE_CODEC_MAX_PACKET (4000).
 * @return Number of bytes written to @p data_out,
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
 * The output is 480 float samples (mono, 48 kHz).
 *
 * @param  dec               Decoder handle.
 * @param  data_in           Compressed Opus packet bytes.
 * @param  data_in_len       Number of bytes in @p data_in.
 *                           Pass 0 to request packet loss concealment (PLC).
 * @param  pcm_out           Output PCM buffer (must hold at least 480 floats).
 * @return Number of samples decoded (typically 480),
 *         or a negative value on error.
 */
VOICE_CODEC_API int voice_decode(
    VoiceDecoder       *dec,
    const uint8_t      *data_in,
    int                 data_in_len,
    float              *pcm_out);

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
