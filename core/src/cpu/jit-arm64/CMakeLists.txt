if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    target_sources(recon_fn PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/analyze.c
        ${CMAKE_CURRENT_LIST_DIR}/backend.c
        ${CMAKE_CURRENT_LIST_DIR}/cache.c
        ${CMAKE_CURRENT_LIST_DIR}/emit.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_alu_imm.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_alu_reg.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_branch.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_cop_ldc.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_cop_swi.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_ldm_stm.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_ldst_imm.c
        ${CMAKE_CURRENT_LIST_DIR}/emit_ldst_reg.c
        ${CMAKE_CURRENT_LIST_DIR}/memhooks.S
    )
elseif(SEEDLESS_LIBRETRO)
    message(STATUS "jit-arm64: skipped (non-AArch64); using libretro/cpu_stub.c")
endif()
