# Makefile for godot-voice — voice compression + noise cancellation
# Builds opus + rnnoise from extern/ submodules, then links our thin wrapper.
# ----------------------------------------------------------------------

CC       := gcc
AR       := ar
CFLAGS   := -O2 -Wall -fPIC
SO_FLAGS := -shared -lm

# Allow override
PREFIX   ?= /usr/local

# ---------------------------------------------------------------------------
# Source file lists
# ---------------------------------------------------------------------------

# --- Opus (reference C only — no x86 SIMD, no DNN/DRED) ---
OPUS_DIR     := extern/opus

# Core (src/)
OPUS_CORE_SRCS := \
	src/opus.c \
	src/opus_decoder.c \
	src/opus_encoder.c \
	src/extensions.c \
	src/opus_multistream.c \
	src/opus_multistream_encoder.c \
	src/opus_multistream_decoder.c \
	src/repacketizer.c \
	src/opus_projection_encoder.c \
	src/opus_projection_decoder.c \
	src/mapping_matrix.c

# Float-API sources (src/)
OPUS_FLOAT_SRCS := \
	src/analysis.c \
	src/mlp.c \
	src/mlp_data.c

# CELT (celt/)
OPUS_CELT_SRCS := \
	celt/bands.c \
	celt/celt.c \
	celt/celt_encoder.c \
	celt/celt_decoder.c \
	celt/cwrs.c \
	celt/entcode.c \
	celt/entdec.c \
	celt/entenc.c \
	celt/kiss_fft.c \
	celt/laplace.c \
	celt/mathops.c \
	celt/mdct.c \
	celt/modes.c \
	celt/pitch.c \
	celt/celt_lpc.c \
	celt/quant_bands.c \
	celt/rate.c \
	celt/vq.c

# SILK (silk/)
OPUS_SILK_SRCS := \
	silk/CNG.c \
	silk/code_signs.c \
	silk/init_decoder.c \
	silk/decode_core.c \
	silk/decode_frame.c \
	silk/decode_parameters.c \
	silk/decode_indices.c \
	silk/decode_pulses.c \
	silk/decoder_set_fs.c \
	silk/dec_API.c \
	silk/enc_API.c \
	silk/encode_indices.c \
	silk/encode_pulses.c \
	silk/gain_quant.c \
	silk/interpolate.c \
	silk/LP_variable_cutoff.c \
	silk/NLSF_decode.c \
	silk/NSQ.c \
	silk/NSQ_del_dec.c \
	silk/PLC.c \
	silk/shell_coder.c \
	silk/tables_gain.c \
	silk/tables_LTP.c \
	silk/tables_NLSF_CB_NB_MB.c \
	silk/tables_NLSF_CB_WB.c \
	silk/tables_other.c \
	silk/tables_pitch_lag.c \
	silk/tables_pulses_per_block.c \
	silk/VAD.c \
	silk/control_audio_bandwidth.c \
	silk/quant_LTP_gains.c \
	silk/VQ_WMat_EC.c \
	silk/HP_variable_cutoff.c \
	silk/NLSF_encode.c \
	silk/NLSF_VQ.c \
	silk/NLSF_unpack.c \
	silk/NLSF_del_dec_quant.c \
	silk/process_NLSFs.c \
	silk/stereo_LR_to_MS.c \
	silk/stereo_MS_to_LR.c \
	silk/check_control_input.c \
	silk/control_SNR.c \
	silk/init_encoder.c \
	silk/control_codec.c \
	silk/A2NLSF.c \
	silk/ana_filt_bank_1.c \
	silk/biquad_alt.c \
	silk/bwexpander_32.c \
	silk/bwexpander.c \
	silk/debug.c \
	silk/decode_pitch.c \
	silk/inner_prod_aligned.c \
	silk/lin2log.c \
	silk/log2lin.c \
	silk/LPC_analysis_filter.c \
	silk/LPC_inv_pred_gain.c \
	silk/table_LSF_cos.c \
	silk/NLSF2A.c \
	silk/NLSF_stabilize.c \
	silk/NLSF_VQ_weights_laroia.c \
	silk/pitch_est_tables.c \
	silk/resampler.c \
	silk/resampler_down2_3.c \
	silk/resampler_down2.c \
	silk/resampler_private_AR2.c \
	silk/resampler_private_down_FIR.c \
	silk/resampler_private_IIR_FIR.c \
	silk/resampler_private_up2_HQ.c \
	silk/resampler_rom.c \
	silk/sigm_Q15.c \
	silk/sort.c \
	silk/sum_sqr_shift.c \
	silk/stereo_decode_pred.c \
	silk/stereo_encode_pred.c \
	silk/stereo_find_predictor.c \
	silk/stereo_quant_pred.c \
	silk/LPC_fit.c

# SILK float (silk/float/)
OPUS_SILK_FLOAT_SRCS := \
	silk/float/apply_sine_window_FLP.c \
	silk/float/corrMatrix_FLP.c \
	silk/float/encode_frame_FLP.c \
	silk/float/find_LPC_FLP.c \
	silk/float/find_LTP_FLP.c \
	silk/float/find_pitch_lags_FLP.c \
	silk/float/find_pred_coefs_FLP.c \
	silk/float/LPC_analysis_filter_FLP.c \
	silk/float/LTP_analysis_filter_FLP.c \
	silk/float/LTP_scale_ctrl_FLP.c \
	silk/float/noise_shape_analysis_FLP.c \
	silk/float/process_gains_FLP.c \
	silk/float/regularize_correlations_FLP.c \
	silk/float/residual_energy_FLP.c \
	silk/float/warped_autocorrelation_FLP.c \
	silk/float/wrappers_FLP.c \
	silk/float/autocorrelation_FLP.c \
	silk/float/burg_modified_FLP.c \
	silk/float/bwexpander_FLP.c \
	silk/float/energy_FLP.c \
	silk/float/inner_product_FLP.c \
	silk/float/k2a_FLP.c \
	silk/float/LPC_inv_pred_gain_FLP.c \
	silk/float/pitch_analysis_core_FLP.c \
	silk/float/scale_copy_vector_FLP.c \
	silk/float/scale_vector_FLP.c \
	silk/float/schur_FLP.c \
	silk/float/sort_FLP.c

# All opus sources with directory prefix
OPUS_SRCS := $(addprefix $(OPUS_DIR)/, \
	$(OPUS_CORE_SRCS) $(OPUS_FLOAT_SRCS) \
	$(OPUS_CELT_SRCS) $(OPUS_SILK_SRCS) $(OPUS_SILK_FLOAT_SRCS))

# --- RNNoise ---
RNNOISE_DIR  := extern/rnnoise

RNNOISE_SRCS := \
	src/celt_lpc.c \
	src/denoise.c \
	src/kiss_fft.c \
	src/nnet.c \
	src/nnet_default.c \
	src/parse_lpcnet_weights.c \
	src/pitch.c \
	src/rnn.c \
	src/rnnoise_data.c \
	src/rnnoise_tables.c

RNNOISE_SRCS := $(addprefix $(RNNOISE_DIR)/, $(RNNOISE_SRCS))

# --- Voice Codec wrapper ---
VOICE_SRCS := \
	src/voice_codec.c \
	src/voice_encoder.c \
	src/voice_decoder.c

# ---------------------------------------------------------------------------
# Object files
# ---------------------------------------------------------------------------
BUILD_DIR   := build/obj
OPUS_OBJS   := $(patsubst $(OPUS_DIR)/%.c,    $(BUILD_DIR)/opus/%.o,    $(OPUS_SRCS))
RNNOISE_OBJS:= $(patsubst $(RNNOISE_DIR)/%.c, $(BUILD_DIR)/rnnoise/%.o, $(RNNOISE_SRCS))
VOICE_OBJS  := $(patsubst src/%.c,            $(BUILD_DIR)/voice/%.o,   $(VOICE_SRCS))

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
OPUS_INC    := -I$(OPUS_DIR)/include \
               -I$(OPUS_DIR) \
               -I$(OPUS_DIR)/celt \
               -I$(OPUS_DIR)/silk \
               -I$(OPUS_DIR)/silk/float \
               -I$(OPUS_DIR)/dnn

RNNOISE_INC := -I$(RNNOISE_DIR)/include \
               -I$(RNNOISE_DIR)/src

VOICE_INC   := -Iinclude

# ---------------------------------------------------------------------------
# Defines
# ---------------------------------------------------------------------------
OPUS_DEFS   := -DOPUS_BUILD -DFLOATING_POINT -DVAR_ARRAYS \
               -DOPUS_X86_MAY_HAVE_SSE -DOPUS_X86_PRESUME_SSE \
               -DOPUS_X86_MAY_HAVE_SSE2 -DOPUS_X86_PRESUME_SSE2 \
               -DOPUS_X86_MAY_HAVE_SSE4_1 -DOPUS_X86_PRESUME_SSE4_1

RNNOISE_DEFS:= -DRNNOISE_BUILD

VOICE_DEFS  := -DVOICE_CODEC_BUILD

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.DEFAULT_GOAL := all
.PHONY: all clean install

all: build/libvoice_codec.so

# --- Static library targets (for linking into .so) ---
build/libopus.a: $(OPUS_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

build/librnnoise.a: $(RNNOISE_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# --- Shared library ---
build/libvoice_codec.so: $(VOICE_OBJS) build/libopus.a build/librnnoise.a
	@mkdir -p $(dir $@)
	$(CC) $(SO_FLAGS) -o $@ $(VOICE_OBJS) -Wl,--whole-archive build/libopus.a build/librnnoise.a -Wl,--no-whole-archive -lm

# --- Pattern rules ---
$(BUILD_DIR)/opus/%.o: $(OPUS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPUS_INC) $(OPUS_DEFS) -c $< -o $@

$(BUILD_DIR)/rnnoise/%.o: $(RNNOISE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RNNOISE_INC) $(RNNOISE_DEFS) -c $< -o $@

$(BUILD_DIR)/voice/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VOICE_INC) $(OPUS_INC) $(RNNOISE_INC) $(VOICE_DEFS) -c $< -o $@

# --- Install ---
install: build/libvoice_codec.so
	install -d $(PREFIX)/lib $(PREFIX)/include
	install -m 755 build/libvoice_codec.so $(PREFIX)/lib/
	install -m 644 include/voice_codec.h $(PREFIX)/include/

# --- Clean ---
clean:
	rm -rf build
