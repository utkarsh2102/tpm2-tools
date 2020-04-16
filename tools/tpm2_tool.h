/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef MAIN_H
#define MAIN_H

#include <tss2/tss2_esys.h>
#include <stdbool.h>

#include "tool_rc.h"
#include "tpm2_options.h"
#include "tpm2_tool_output.h"

/**
 * An optional interface for tools to specify what options they support.
 * They are concatenated with main's options and passed to getopt_long.
 * @param opts
 *  The callee can choose to set *opts to a tpm_options pointer allocated
 *  via tpm2_options_new(). Setting *opts to NULL is not an error, and
 *  Indicates that no options are specified by the tool.
 *
 * @return
 *  True on success, false on error.
 */
bool tpm2_tool_onstart(tpm2_options **opts) __attribute__((weak));

/**
 * This is the main interface for tools, after tcti and sapi/esapi initialization
 * are performed.
 * @param ectx
 *  The system/esapi api context.
 * @param flags
 *  Flags that tools may wish to respect.
 * @return
 *  A tool_rc indicating status.
 */
tool_rc tpm2_tool_onrun(ESYS_CONTEXT *ectx, tpm2_option_flags flags) __attribute__((weak));

/**
 * Called after tpm2_tool_onrun() is invoked. ESAPI context is still valid during this call.
 * @param ectx
 *  The system/esapi api context.
 * @return
 *  A tool_rc indicating status.
 */
tool_rc tpm2_tool_onstop(ESYS_CONTEXT *ectx) __attribute__((weak));

/**
 * Called when the tool is exiting, useful for cleanup.
 */
void tpm2_tool_onexit(void) __attribute__((weak));

#endif /* MAIN_H */
