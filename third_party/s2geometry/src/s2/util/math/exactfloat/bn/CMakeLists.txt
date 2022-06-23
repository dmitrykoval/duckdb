project(crypto C)

set(SSL_CRYPTO
        ${S2_SOURCE_DIR}/s2/util/math/exactfloat/bn/bn.c
        ${S2_SOURCE_DIR}/s2/util/math/exactfloat/bn/bn_asm.c
        ${S2_SOURCE_DIR}/s2/util/math/exactfloat/bn/bn_ctx.c
        ${S2_SOURCE_DIR}/s2/util/math/exactfloat/bn/bn_mul.c
        ${S2_SOURCE_DIR}/s2/util/math/exactfloat/bn/bn_sqr.c
        )

add_library(duckdb_crypto STATIC ${SSL_CRYPTO})
set_target_properties(duckdb_crypto PROPERTIES LINKER_LANGUAGE C)
target_include_directories(duckdb_crypto PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/src/s2/util/math/exactfloat/bn")

disable_target_warnings(duckdb_crypto)
