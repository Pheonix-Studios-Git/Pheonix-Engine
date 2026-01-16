#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <decoders/unicode.h>

uint32_t px_utf8_decode(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    uint32_t cp;

    if (p[0] < 0x80) {
        cp = p[0];
        *s += 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *s += 2;
    } else if ((p[0] & 0xF0) == 0xE0) {
        cp = ((p[0] & 0x0F) << 12) |
             ((p[1] & 0x3F) << 6) |
             (p[2] & 0x3F);
        *s += 3;
    } else {
        cp = '?';
        *s += 1;
    }
    return cp;
}

