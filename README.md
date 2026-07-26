# libvoice

**Voice compression with automatic noise cancellation** — a thin C library combining **[Xiph Opus](https://opus-codec.org/)** and **[Xiph RNNoise](https://github.com/xiph/rnnoise)** into a single, easy-to-use API. Designed for clean interop from C# (P/Invoke) and other managed languages.

## How it works

```
🎤 Mic PCM (float[480])          📦 Compressed bytes (Opus)
   → RNNoise denoise (optional)     → Opus decode
   → Opus encode (32 kbps)         → PCM float[480]
   → compressed bytes              → 🔊 Speaker
```

- **Noise suppression**: RNNoise neural-network denoiser removes background noise *before* encoding
- **Compression**: Opus codec tuned for voice (VOIP mode), ~32 kbps typical
- **Fixed format**: 48 kHz mono, 10 ms frames (480 samples), 32-bit float PCM

## Building

Prebuilt binaries are attached to [GitHub Releases](https://github.com/KitDoesIt/libvoice/releases).

### Linux
```bash
git clone --recurse-submodules https://github.com/KitDoesIt/libvoice.git
cd libvoice
cd extern/rnnoise && bash download_model.sh && cd ../..
make
# → build/libvoice_codec.so
```
Requires: `gcc`, `make`, `ar`, `wget`.

### Windows (cross-compile from Linux)
```bash
sudo apt install gcc-mingw-w64-x86-64
make windows
# → build/libvoice_codec.dll
```

## API

All symbols are prefixed `voice_`. See [`include/voice_codec.h`](include/voice_codec.h) for full details.

### Constants

```c
#define VOICE_CODEC_SAMPLE_RATE  48000   // Hz
#define VOICE_CODEC_FRAME_SIZE   480     // samples (10 ms)
#define VOICE_CODEC_MAX_PACKET   4000    // worst-case Opus frame
```

### Encoder

```c
VoiceEncoder *voice_encoder_create(int bitrate, int denoise_enabled);
void          voice_encoder_destroy(VoiceEncoder *enc);
int           voice_encode(VoiceEncoder *enc,
                           const float *pcm_in,
                           uint8_t *data_out,
                           int data_out_capacity);
int           voice_encode_flush(VoiceEncoder *enc,
                                  uint8_t *data_out,
                                  int data_out_capacity);
```

- `bitrate` — target Opus bitrate in bps (e.g. `32000`). Pass `0` for Opus default.
- `denoise_enabled` — set to `1` to run RNNoise before encoding.
- `voice_encode` returns the number of bytes written, or negative on error.
- `voice_encode_flush` emits any DTX tail frames; returns 0 if nothing to flush.

### Decoder

```c
VoiceDecoder *voice_decoder_create(int denoise_enabled);
void          voice_decoder_destroy(VoiceDecoder *dec);
int           voice_decode(VoiceDecoder *dec,
                           const uint8_t *data_in,
                           int data_in_len,
                           float *pcm_out);
```

- Pass `data_in_len = 0` for **packet loss concealment** (PLC).
- Returns the number of samples decoded (typically 480), or negative on error.

### Utility

```c
int         voice_get_frame_size(void);   // 480
int         voice_get_sample_rate(void);  // 48000
const char *voice_get_version(void);      // version string
```

## C# usage (P/Invoke)

```csharp
using System;
using System.Runtime.InteropServices;

public static class VoiceCodec
{
    public const int SampleRate  = 48000;
    public const int FrameSize   = 480;
    public const int MaxPacket   = 4000;

    [DllImport("voice_codec")] public static extern IntPtr voice_encoder_create(int bitrate, int denoiseEnabled);
    [DllImport("voice_codec")] public static extern void   voice_encoder_destroy(IntPtr enc);
    [DllImport("voice_codec")] public static extern int    voice_encode(IntPtr enc, float[] pcmIn, byte[] dataOut, int capacity);

    [DllImport("voice_codec")] public static extern IntPtr voice_decoder_create(int denoiseEnabled);
    [DllImport("voice_codec")] public static extern void   voice_decoder_destroy(IntPtr dec);
    [DllImport("voice_codec")] public static extern int    voice_decode(IntPtr dec, byte[] dataIn, int len, float[] pcmOut);
}
```

```csharp
// Encode a frame
var enc = VoiceCodec.voice_encoder_create(32000, denoiseEnabled: 1);
var pcm = new float[VoiceCodec.FrameSize];
var packet = new byte[VoiceCodec.MaxPacket];

// … fill pcm from your mic …

int bytes = VoiceCodec.voice_encode(enc, pcm, packet, packet.Length);

// Decode
var dec = VoiceCodec.voice_decoder_create(denoiseEnabled: 0);
var decoded = new float[VoiceCodec.FrameSize];

int samples = VoiceCodec.voice_decode(dec, packet, bytes, decoded);

VoiceCodec.voice_encoder_destroy(enc);
VoiceCodec.voice_decoder_destroy(dec);
```

## Project structure

```
libvoice/
├── extern/
│   ├── opus/              # Xiph Opus (git submodule)
│   └── rnnoise/           # Xiph RNNoise (git submodule)
├── include/
│   └── voice_codec.h      # Public API header
├── src/
│   ├── voice_codec.c      # Version / utility functions
│   ├── voice_encoder.c    # RNNoise → Opus pipeline
│   └── voice_decoder.c    # Opus → PCM pipeline
├── Makefile               # Builds libvoice_codec.so
└── CMakeLists.txt         # Alternative CMake build
```

## License

All code in this repository is [3-clause BSD](LICENSE), matching [Opus](https://github.com/xiph/opus) and [RNNoise](https://github.com/xiph/rnnoise).
