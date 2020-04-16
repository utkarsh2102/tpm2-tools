/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "tools/fapi/tss2_template.h"

/* needed by tpm2_util and tpm2_option functions */
bool output_enabled = false;

/* Context struct used to store passed command line parameters */
static struct cxt {
    char   const *nvPath;
    char   const *data;
} ctx;

/* Parse command line parameters */
static bool on_option(char key, char *value) {
    switch (key) {
    case 'i':
        ctx.data = value;
        break;
    case 'p':
        ctx.nvPath = value;
        break;
    }
    return true;
}

/* Define possible command line parameters */
bool tss2_tool_onstart(tpm2_options **opts) {
    struct option topts[] = {
        {"data"  , required_argument, NULL, 'i'},
        {"nvPath"  , required_argument, NULL, 'p'}
    };
    return (*opts = tpm2_options_new ("i:p:", ARRAY_LEN(topts), topts,
                                      on_option, NULL, 0)) != NULL;
}

/* Execute specific tool */
int tss2_tool_onrun (FAPI_CONTEXT *fctx) {
    /* Check availability of required parameters */
    if (!ctx.nvPath) {
        fprintf (stderr, "No NV path provided, use --nvPath\n");
        return -1;
    }
    if (!ctx.data) {
        fprintf (stderr, "No file for output provided, use --data\n");
        return -1;
    }

    /* Read data file */
    uint8_t *data;
    size_t data_len;
    TSS2_RC r = open_read_and_close (ctx.data, (void**)&data, &data_len);
    if (r) {
        LOG_PERR ("open_read_and_close data", r);
        return 1;
    }

    /* Execute FAPI command with passed arguments */
    r = Fapi_NvWrite(fctx, ctx.nvPath, data, data_len);
    if (r != TSS2_RC_SUCCESS){
        LOG_PERR ("Fapi_NvWrite", r);
        return 1;
    }
    Fapi_Free (data);
    return 0;
}
