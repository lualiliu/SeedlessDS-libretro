if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    target_sources(recon_fn PRIVATE ${CMAKE_CURRENT_LIST_DIR}/adpcm.S)
elseif(SEEDLESS_LIBRETRO)
    # C fallback supplied by the libretro frontend target.
    message(STATUS "spu/adpcm.S: skipped (non-AArch64)")
endif()
target_sources(recon_fn PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/adpcm_tables.c
    ${CMAKE_CURRENT_LIST_DIR}/mixer.c
    ${CMAKE_CURRENT_LIST_DIR}/resample.c
    ${CMAKE_CURRENT_LIST_DIR}/spu.c
)
