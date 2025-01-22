set(SOURCES_C
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/srtp/srtp.c
)

set(CIPHERS_SOURCES_C
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/cipher.c
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/null_cipher.c
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/cipher_test_cases.c
)


if (ENABLE_OPENSSL)
  list(APPEND CIPHERS_SOURCES_C
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/aes_icm_ossl.c
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/aes_gcm_ossl.c
  )
else()
  list(APPEND  CIPHERS_SOURCES_C
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/aes.c
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/cipher/aes_icm.c
  )
endif()


set(HASHES_SOURCES_C
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/hash/auth.c
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/hash/null_auth.c
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/hash/auth_test_cases.c
)


if (ENABLE_OPENSSL)
  list(APPEND HASHES_SOURCES_C
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/hash/hmac_ossl.c
  )
else()
  list(APPEND  HASHES_SOURCES_C
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/hash/hmac.c
    ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/hash/sha1.c
  )
endif()


set(KERNEL_SOURCES_C
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/kernel/alloc.c
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/kernel/crypto_kernel.c
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/kernel/err.c
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/kernel/key.c
)


set(MATH_SOURCES_C
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/math/datatypes.c
)


set(REPLAY_SOURCES_C
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/replay/rdb.c
  ${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/replay/rdbx.c
)

set(SRTP2_SRCS
  ${SOURCES_C}
  ${CIPHERS_SOURCES_C}
  ${HASHES_SOURCES_C}
  ${KERNEL_SOURCES_C}
  ${MATH_SOURCES_C}
  ${REPLAY_SOURCES_C}
)

set(SRTP2_INCLUDE_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/port
	${CMAKE_CURRENT_LIST_DIR}/libsrtp/include
	${CMAKE_CURRENT_LIST_DIR}/libsrtp/crypto/include
	)