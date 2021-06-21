#include <math.h>
#include <string.h>
#include <assert.h>

#include "dump978/uat.h"
#include "dump978/uat_decode.h"

void dump_raw_message(char updown, uint8_t* data, int len, int rs_errors)
{
    static struct uat_adsb_mdb mdb_zero;

    fprintf(stdout, "%c", updown);
    for (int i = 0; i < len; ++i) {
        fprintf(stdout, "%02x", data[i]);
    }

    if (rs_errors)
        fprintf(stdout, ";rs=%d", rs_errors);
    fprintf(stdout, ";\n");

    uat_decode_adsb_mdb(data, &mdb_zero);
}

