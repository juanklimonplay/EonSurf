#ifndef EONSURF_JA3_H
#define EONSURF_JA3_H

#include <mbedtls/ssl.h>
#include <mbedtls/ecp.h>

static const int eonsurf_ja3_ciphersuites[] = {
#if defined(MBEDTLS_TLS1_3_AES_128_GCM_SHA256)
    MBEDTLS_TLS1_3_AES_128_GCM_SHA256,
    MBEDTLS_TLS1_3_AES_256_GCM_SHA384,
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
#endif
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    0
};

static const mbedtls_ecp_group_id eonsurf_ja3_curves[] = {
    MBEDTLS_ECP_DP_CURVE25519,
    MBEDTLS_ECP_DP_SECP256R1,
    MBEDTLS_ECP_DP_SECP384R1,
    MBEDTLS_ECP_DP_NONE
};

static const char *eonsurf_alpn_protocols[] = {
    "http/1.1",
    NULL
};

static inline void eonsurf_apply_ja3_spoof(mbedtls_ssl_config *conf)
{
    mbedtls_ssl_conf_ciphersuites(conf, eonsurf_ja3_ciphersuites);
    mbedtls_ssl_conf_curves(conf, eonsurf_ja3_curves);
#if defined(MBEDTLS_SSL_ALPN)
    mbedtls_ssl_conf_alpn_protocols(conf, eonsurf_alpn_protocols);
#endif
    mbedtls_ssl_conf_min_version(conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
}

#endif /* EONSURF_JA3_H */
