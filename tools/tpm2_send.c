/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "log.h"
#include "tpm2_header.h"
#include "tpm2_tool.h"

typedef struct tpm2_send_ctx tpm2_send_ctx;
struct tpm2_send_ctx {
    FILE *input;
    FILE *output;
};

static tpm2_send_ctx ctx;

static bool read_command_from_file(FILE *f, tpm2_command_header **c,
        UINT32 *size) {

    UINT8 buffer[TPM2_COMMAND_HEADER_SIZE];

    size_t ret = fread(buffer, TPM2_COMMAND_HEADER_SIZE, 1, f);
    if (ret != 1 && ferror(f)) {
        LOG_ERR("Failed to read command header: %s", strerror (errno));
        return false;
    }

    tpm2_command_header *header = tpm2_command_header_from_bytes(buffer);

    UINT32 command_size = tpm2_command_header_get_size(header, true);
    UINT32 data_size = tpm2_command_header_get_size(header, false);

    if (command_size > TPM2_MAX_SIZE || command_size < data_size) {
        LOG_ERR("Command buffer %"PRIu32" bytes cannot be smaller then the "
                "encapsulated data %"PRIu32" bytes, and can not be bigger than"
                " the maximum buffer size", command_size, data_size);
        return false;
    }

    tpm2_command_header *command = (tpm2_command_header *) malloc(command_size);
    if (!command) {
        LOG_ERR("oom");
        return false;
    }

    /* copy the header into the struct */
    memcpy(command, buffer, sizeof(buffer));

    LOG_INFO("command tag:  0x%04x", tpm2_command_header_get_tag(command));
    LOG_INFO("command size: 0x%08x", command_size);
    LOG_INFO("command code: 0x%08x", tpm2_command_header_get_code(command));

    ret = fread(command->data, data_size, 1, f);
    if (ret != 1 && ferror(f)) {
        LOG_ERR("Failed to read command body: %s", strerror (errno));
        free(command);
        return false;
    }

    *c = command;
    *size = command_size;

    return true;
}

static bool write_response_to_file(FILE *f, UINT8 *rbuf) {

    tpm2_response_header *r = tpm2_response_header_from_bytes(rbuf);

    UINT32 size = tpm2_response_header_get_size(r, true);

    LOG_INFO("response tag:  0x%04x", tpm2_response_header_get_tag(r));
    LOG_INFO("response size: 0x%08x", size);
    LOG_INFO("response code: 0x%08x", tpm2_response_header_get_code(r));

    return files_write_bytes(f, r->bytes, size);
}

static FILE *open_file(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    if (!f) {
        LOG_ERR("Could not open \"%s\", error: \"%s\"", path, strerror(errno));
    }
    return f;
}

static void close_file(FILE *f) {

    if (f && (f != stdin || f != stdout)) {
        fclose(f);
    }
}

static bool on_option(char key, char *value) {

    switch (key) {
    case 'o':
        ctx.output = open_file(value, "wb");
        if (!ctx.output) {
            return false;
        }
        break;
        /* no break */
    }

    return true;
}

static bool on_args(int argc, char **argv) {

    if (argc > 1) {
        LOG_ERR("Expected 1 tpm buffer input file, got: %d", argc);
        return false;
    }

    ctx.input = fopen(argv[0], "rb");
    if (!ctx.input) {
        LOG_ERR("Error opening file \"%s\", error: %s", argv[0],
                strerror(errno));
        return false;
    }

    return true;
}

bool tpm2_tool_onstart(tpm2_options **opts) {

    static const struct option topts[] = {
        { "output", required_argument, NULL, 'o' },
    };

    *opts = tpm2_options_new("o:", ARRAY_LEN(topts), topts, on_option, on_args,
            0);

    ctx.input = stdin;
    ctx.output = stdout;

    return *opts != NULL;
}

/*
 * This program reads a TPM command buffer from stdin then dumps it out
 * to a tabd TCTI. It then reads the response from the TCTI and writes it
 * to stdout. Like the TCTI, we expect the input TPM command buffer to be
 * in network byte order (big-endian). We output the response in the same
 * form.
 */
tool_rc tpm2_tool_onrun(ESYS_CONTEXT *context, tpm2_option_flags flags) {

    UNUSED(flags);

    tool_rc rc = tool_rc_general_error;

    UINT32 size;
    tpm2_command_header *command;
    bool result = read_command_from_file(ctx.input, &command, &size);
    if (!result) {
        LOG_ERR("failed to read TPM2 command buffer from file");
        goto out_files;
    }

    TSS2_TCTI_CONTEXT *tcti_context;
    TSS2_RC rval = Esys_GetTcti(context, &tcti_context);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_GetTctiContext, rval);
        rc = tool_rc_from_tpm(rval);
        goto out;
    }

    rval = Tss2_Tcti_Transmit(tcti_context, size, command->bytes);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_ERR("tss2_tcti_transmit failed: 0x%x", rval);
        goto out;
    }

    size_t rsize = TPM2_MAX_SIZE;
    UINT8 rbuf[TPM2_MAX_SIZE];
    rval = Tss2_Tcti_Receive(tcti_context, &rsize, rbuf,
            TSS2_TCTI_TIMEOUT_BLOCK);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_ERR("tss2_tcti_receive failed: 0x%x", rval);
        rc = tool_rc_from_tpm(rval);
        goto out;
    }

    /*
     * The response buffer, rbuf, all fields are in big-endian, and we save
     * in big-endian.
     */
    result = write_response_to_file(ctx.output, rbuf);
    if (!result) {
        LOG_ERR("Failed writing response to output file.");
    } else {
        rc = tool_rc_success;
    }

out:
    free(command);

out_files:
    close_file(ctx.input);
    close_file(ctx.output);

    return rc;
}
