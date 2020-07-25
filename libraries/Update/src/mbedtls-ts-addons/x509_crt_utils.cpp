#include <string.h>
#include <assert.h>

#include "mbedtls/platform.h"

#include "x509_ts_utils.h"
#include "x509_crt_utils.h"

#ifndef MIN
#define MIN(a,b) ( ((a) <(b)) ? (a) : (b))
#endif

int mbedtls_x509_crt_fprint(char * buff, size_t len, const char * prefix, const mbedtls_x509_crt * cert, mbedtls_md_type_t tpe) 
{
    const mbedtls_md_info_t * mdt = mbedtls_md_info_from_type(tpe ? tpe : MBEDTLS_MD_SHA256);
    const char * nme = mbedtls_md_get_name(mdt);
    const size_t dl = mbedtls_md_get_size(mdt);
    const char * txt = " fingerprint: ";

    unsigned char *output = NULL;
    char *p = buff, * ep = buff + len-4;
    mbedtls_md_context_t ctx;
    int i, ret = -1;
    size_t l;
    
    mbedtls_md_init(&ctx);
    if ( 
        ((output = (unsigned char *)mbedtls_calloc(dl,1)) == NULL) ||
        ((ret = mbedtls_md_setup(&ctx, mdt, 9)) != 0) ||
        ((ret = mbedtls_md_starts (&ctx)) != 0) ||
        ((ret = mbedtls_md_update (&ctx, cert->raw.p, cert->raw.len)) != 0) ||
        ((ret = mbedtls_md_finish (&ctx, output)) != 0)
        ) {
         goto erx;
    };

    p = buff;
    if (prefix) {
        l = MIN(strlen(prefix), ep-p);
        memcpy(p, prefix, l);
        p += l;
    };

    l = MIN(strlen(nme), ep-p);
    memcpy(p, nme, l);
    p += l;
    
    l = MIN(strlen(txt), ep-p);
    memcpy(p, txt, l);
    p += l;

    for(i = 0; i < dl; i++) {
        int c1 = output[i] >> 4;
        int c2 = output[i] & 0xF;
        if (p < ep)
            *(p++) = (c1 < 10) ? '0' + c1 : 'A' + (c1 - 10);
        if (p < ep)
            *(p++) = (c2 < 10) ? '0' + c2 : 'A' + (c2 - 10);
        if (p < ep && i != dl-1)
            *(p++) = ':';
    };

    // Add 3 ominous dots, like ellipses, if our buffer falls short.
    //
    if (p == ep) {
        *(p++)='.';*(p++)='.';*(p++)='.';
    };
    *(p++)= '\0';

    // return the bytes written into buff.
    ret = p - buff; 
erx:
    mbedtls_md_free(&ctx);
    if (output) mbedtls_free(output);

    return ret;
}

