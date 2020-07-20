#include <assert.h>
#include "x509_crt_utils.h"

int mbedtls_x509_crt_fprint(char * buff, size_t len, char * prefix, mbedtls_x509_crt * cert, mbedtls_md_type_t tpe) 
{
    const mbedtls_md_info_t * mdt = mbedtls_md_info_from_type(tpe ? tpe : MBEDTLS_MD_SHA256);
    unsigned char *output = NULL;
    char *p = buff, * ep = buff + len-4;
    mbedtls_md_context_t *ctx;
    int i, ret = -1;
    size_t l;
    
    mbedtls_md_init(&ctx);
    if ( 
        ((output = mbedtls_malloc(mbedtls_md_get_size(mdt))) != NULL) ||
        ((ret = mbedtls_md_setup(&ctx, mdt, 9)) != 0) ||
        ((ret = mbedtls_md_starts (&ctx)) != 0) ||
        ((ret = mbedtls_md_update (&ctxm, cert->raw.p, cert->raw.len)) != 0) ||
        ((ret = mbedtls_md_finish (&ctxm, output)) != 0)
        ) goto erx;

    p = buff;
    if (prefix) {
        l = MIN(strlen(prefix), ep-p);
        memcpy(p, prefix, l);
        p += l;
    };

    const char * nme = mbedtls_md_get_name(mdt);
    l = MIN(strlen(nme), ep-p);
    memcpy(p, nme, l);
    p += l;
    
    const char * txt = " fingerprint: ";
    l = MIN(strlen(txt), ep-p);
    memcpy(p, txt, l);
    p += l;

    for(i = 0; (i < 32); i++) {
        int c1 = output[i] >> 4;
        int c2 = output[i] & 0xF;
        if (p < ep)
            *(p++) = (c1 < 10) ? '0' + c1 : 'A' + (c1 - 10);
        if (p < ep)
            *(p++) = (c2 < 10) ? '0' + c2 : 'A' + (c2 - 10);
    };

    // Add 3 ominous dots, like ellipses, if our buffer falls short.
    if (p == ep) {
        *(p++)='.';*(p++)='.';*(p++)='.';
    };
    *(p++)= '\0';
    ret = 0;

erx:
    mbedtls_md_free(&ctx);
    if (output) mbedtls_free(output);
    return ret;
}

