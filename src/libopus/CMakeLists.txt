##
# @file CMakeLists.txt
# @brief 
#/

# MODULE_PATH
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# MODULE_NAME
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

set(CELT_SOURCES
	"${MODULE_PATH}/opus/celt/bands.c"
	"${MODULE_PATH}/opus/celt/celt.c"
	"${MODULE_PATH}/opus/celt/celt_encoder.c"
	"${MODULE_PATH}/opus/celt/celt_decoder.c"
	"${MODULE_PATH}/opus/celt/cwrs.c"
	"${MODULE_PATH}/opus/celt/entcode.c"
	"${MODULE_PATH}/opus/celt/entdec.c"
	"${MODULE_PATH}/opus/celt/entenc.c"
	"${MODULE_PATH}/opus/celt/kiss_fft.c"
	"${MODULE_PATH}/opus/celt/laplace.c"
	"${MODULE_PATH}/opus/celt/mathops.c"
	"${MODULE_PATH}/opus/celt/mdct.c"
	"${MODULE_PATH}/opus/celt/modes.c"
	"${MODULE_PATH}/opus/celt/pitch.c"
	"${MODULE_PATH}/opus/celt/celt_lpc.c"
	"${MODULE_PATH}/opus/celt/quant_bands.c"
	"${MODULE_PATH}/opus/celt/rate.c"
	"${MODULE_PATH}/opus/celt/vq.c")

set(OPUS_SOURCES 
    "${MODULE_PATH}/opus/src/opus.c"
    "${MODULE_PATH}/opus/src/opus_decoder.c"
	"${MODULE_PATH}/opus/src/opus_encoder.c"
    "${MODULE_PATH}/opus/src/extensions.c"
	"${MODULE_PATH}/opus/src/opus_multistream.c"
	"${MODULE_PATH}/opus/src/opus_multistream_encoder.c"
	"${MODULE_PATH}/opus/src/opus_multistream_decoder.c"
	"${MODULE_PATH}/opus/src/repacketizer.c"
	"${MODULE_PATH}/opus/src/opus_projection_encoder.c"
	"${MODULE_PATH}/opus/src/opus_projection_decoder.c"
	"${MODULE_PATH}/opus/src/mapping_matrix.c")

set(SILK_SOURCES
    "${MODULE_PATH}/opus/silk/CNG.c"
    "${MODULE_PATH}/opus/silk/code_signs.c"
    "${MODULE_PATH}/opus/silk/init_decoder.c"
    "${MODULE_PATH}/opus/silk/decode_core.c"
    "${MODULE_PATH}/opus/silk/decode_frame.c"
    "${MODULE_PATH}/opus/silk/decode_parameters.c"
    "${MODULE_PATH}/opus/silk/decode_indices.c"
    "${MODULE_PATH}/opus/silk/decode_pulses.c"
    "${MODULE_PATH}/opus/silk/decoder_set_fs.c"
    "${MODULE_PATH}/opus/silk/dec_API.c"
    "${MODULE_PATH}/opus/silk/enc_API.c"
    "${MODULE_PATH}/opus/silk/encode_indices.c"
    "${MODULE_PATH}/opus/silk/encode_pulses.c"
    "${MODULE_PATH}/opus/silk/gain_quant.c"
    "${MODULE_PATH}/opus/silk/interpolate.c"
    "${MODULE_PATH}/opus/silk/LP_variable_cutoff.c"
    "${MODULE_PATH}/opus/silk/NLSF_decode.c"
    "${MODULE_PATH}/opus/silk/NSQ.c"
    "${MODULE_PATH}/opus/silk/NSQ_del_dec.c"
    "${MODULE_PATH}/opus/silk/PLC.c"
    "${MODULE_PATH}/opus/silk/shell_coder.c"
    "${MODULE_PATH}/opus/silk/tables_gain.c"
    "${MODULE_PATH}/opus/silk/tables_LTP.c"
    "${MODULE_PATH}/opus/silk/tables_NLSF_CB_NB_MB.c"
    "${MODULE_PATH}/opus/silk/tables_NLSF_CB_WB.c"
    "${MODULE_PATH}/opus/silk/tables_other.c"
    "${MODULE_PATH}/opus/silk/tables_pitch_lag.c"
    "${MODULE_PATH}/opus/silk/tables_pulses_per_block.c"
    "${MODULE_PATH}/opus/silk/VAD.c"
    "${MODULE_PATH}/opus/silk/control_audio_bandwidth.c"
    "${MODULE_PATH}/opus/silk/quant_LTP_gains.c"
    "${MODULE_PATH}/opus/silk/VQ_WMat_EC.c"
    "${MODULE_PATH}/opus/silk/HP_variable_cutoff.c"
    "${MODULE_PATH}/opus/silk/NLSF_encode.c"
    "${MODULE_PATH}/opus/silk/NLSF_VQ.c"
    "${MODULE_PATH}/opus/silk/NLSF_unpack.c"
    "${MODULE_PATH}/opus/silk/NLSF_del_dec_quant.c"
    "${MODULE_PATH}/opus/silk/process_NLSFs.c"
    "${MODULE_PATH}/opus/silk/stereo_LR_to_MS.c"
    "${MODULE_PATH}/opus/silk/stereo_MS_to_LR.c"
    "${MODULE_PATH}/opus/silk/check_control_input.c"
    "${MODULE_PATH}/opus/silk/control_SNR.c"
    "${MODULE_PATH}/opus/silk/init_encoder.c"
    "${MODULE_PATH}/opus/silk/control_codec.c"
    "${MODULE_PATH}/opus/silk/A2NLSF.c"
    "${MODULE_PATH}/opus/silk/ana_filt_bank_1.c"
    "${MODULE_PATH}/opus/silk/biquad_alt.c"
    "${MODULE_PATH}/opus/silk/bwexpander_32.c"
    "${MODULE_PATH}/opus/silk/bwexpander.c"
    "${MODULE_PATH}/opus/silk/debug.c"
    "${MODULE_PATH}/opus/silk/decode_pitch.c"
    "${MODULE_PATH}/opus/silk/inner_prod_aligned.c"
    "${MODULE_PATH}/opus/silk/lin2log.c"
    "${MODULE_PATH}/opus/silk/log2lin.c"
    "${MODULE_PATH}/opus/silk/LPC_analysis_filter.c"
    "${MODULE_PATH}/opus/silk/LPC_inv_pred_gain.c"
    "${MODULE_PATH}/opus/silk/table_LSF_cos.c"
    "${MODULE_PATH}/opus/silk/NLSF2A.c"
    "${MODULE_PATH}/opus/silk/NLSF_stabilize.c"
    "${MODULE_PATH}/opus/silk/NLSF_VQ_weights_laroia.c"
    "${MODULE_PATH}/opus/silk/pitch_est_tables.c"
    "${MODULE_PATH}/opus/silk/resampler.c"
    "${MODULE_PATH}/opus/silk/resampler_down2_3.c"
    "${MODULE_PATH}/opus/silk/resampler_down2.c"
    "${MODULE_PATH}/opus/silk/resampler_private_AR2.c"
    "${MODULE_PATH}/opus/silk/resampler_private_down_FIR.c"
    "${MODULE_PATH}/opus/silk/resampler_private_IIR_FIR.c"
    "${MODULE_PATH}/opus/silk/resampler_private_up2_HQ.c"
    "${MODULE_PATH}/opus/silk/resampler_rom.c"
    "${MODULE_PATH}/opus/silk/sigm_Q15.c"
    "${MODULE_PATH}/opus/silk/sort.c"
    "${MODULE_PATH}/opus/silk/sum_sqr_shift.c"
    "${MODULE_PATH}/opus/silk/stereo_decode_pred.c"
    "${MODULE_PATH}/opus/silk/stereo_encode_pred.c"
    "${MODULE_PATH}/opus/silk/stereo_find_predictor.c"
    "${MODULE_PATH}/opus/silk/stereo_quant_pred.c"
    "${MODULE_PATH}/opus/silk/LPC_fit.c"
)

set(SILK_SOURCES_FIXED
    "${MODULE_PATH}/opus/silk/fixed/LTP_analysis_filter_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/LTP_scale_ctrl_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/corrMatrix_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/encode_frame_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/find_LPC_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/find_LTP_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/find_pitch_lags_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/find_pred_coefs_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/noise_shape_analysis_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/process_gains_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/regularize_correlations_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/residual_energy16_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/residual_energy_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/warped_autocorrelation_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/apply_sine_window_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/autocorr_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/burg_modified_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/k2a_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/k2a_Q16_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/pitch_analysis_core_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/vector_ops_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/schur64_FIX.c"
    "${MODULE_PATH}/opus/silk/fixed/schur_FIX.c"
)

# LIB_SRCS
set(LIB_SRCS
    ${CELT_SOURCES} 
    ${OPUS_SOURCES}
    ${SILK_SOURCES} 
    ${SILK_SOURCES_FIXED}
)

# LIB_PUBLIC_INC
set(LIB_PUBLIC_INC
    ${MODULE_PATH}/opus/include
)

# LIB_PRIVATE_INC
set(LIB_PRIVATE_INC
    ${MODULE_PATH}/opus/celt
    ${MODULE_PATH}/opus/silk/fixed
    ${MODULE_PATH}/opus/src
    ${MODULE_PATH}/opus/silk/
)

set(LIB_OPTIONS "-w")
list(APPEND LIB_OPTIONS -DHAVE_ALLOCA_H -DHAVE_LRINT 
    -DHAVE_LRINTF -DFIXED_POINT -DDISABLE_FLOAT_API -DHAVE_MEMORY_H -DUSE_ALLOCA
    -DOPUS_BUILD)
########################################
# Target Configure
########################################
add_library(${MODULE_NAME})

target_sources(${MODULE_NAME}
    PRIVATE
        ${LIB_SRCS}
    )

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${LIB_PRIVATE_INC}

    PUBLIC
        ${LIB_PUBLIC_INC}
    )

target_compile_options(${MODULE_NAME}
    PRIVATE
        ${LIB_OPTIONS}
    PUBLIC
        ${LIB_PUBLIC_OPTIONS}
    )
########################################
# Layer Configure
########################################
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
