/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tools/fapi/tss2_template.h"

/* needed by tpm2_util and tpm2_option functions */
bool output_enabled = false;

/* Context struct used to store passed commandline parameters */
static struct cxt {
    char const *appData;
    char    const *path;
} ctx;

/* Parse commandline parameters */
static bool on_option(char key, char *value) {
    switch (key) {
    case 'i':
        ctx.appData = value;
        break;
    case 'p':
        ctx.path = value;
        break;
    }
    return true;
}

/* Define possible commandline parameters */
bool tss2_tool_onstart(tpm2_options **opts) {
    struct option topts[] = {
        {"appData", required_argument, NULL, 'i'},
        {"path", required_argument, NULL, 'p'},
    };
    return (*opts = tpm2_options_new ("i:p:", ARRAY_LEN(topts), topts,
                                      on_option, NULL, 0)) != NULL;
}

/* Execute specific tool */
int tss2_tool_onrun (FAPI_CONTEXT *fctx) {
    /* Check availability of required parameters */
    if (!ctx.path) {
        fprintf (stderr, "path is missing, use --path\n");
        return -1;
    }

    /* Read appData from file */
    TSS2_RC r;
    uint8_t* appData = NULL;
    size_t appDataSize = 0;
    if (ctx.appData) {
        r = open_read_and_close (ctx.appData, (void**)&appData,
            &appDataSize);
        if (r) {
            return 1;
        }
    }

    /* Execute FAPI command with passed arguments */
    r = Fapi_SetAppData (fctx, ctx.path, appData, appDataSize);
    if (r != TSS2_RC_SUCCESS) {
        LOG_PERR ("Fapi_SetAppData", r);
        free(appData);
        return 1;
    }
    free(appData);
    return 0;
}
