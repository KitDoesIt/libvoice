/**
 * @file voice_codec.c
 * @brief Utility functions for the voice codec library.
 */

#include <stdio.h>
#include <opus.h>
#include "voice_codec.h"

int voice_get_frame_size(void)
{
    return VOICE_CODEC_FRAME_SIZE;
}

int voice_get_sample_rate(void)
{
    return VOICE_CODEC_SAMPLE_RATE;
}

const char *voice_get_version(void)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "godot-voice 0.1.0 (%s)",
             opus_get_version_string());
    return buf;
}
