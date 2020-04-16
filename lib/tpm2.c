/* SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>
#include <tss2/tss2_mu.h>
#include <tss2/tss2_sys.h>

#include "log.h"
#include "object.h"
#include "tool_rc.h"
#include "tpm2.h"
#include "tpm2_alg_util.h"
#include "tpm2_auth_util.h"
#include "tpm2_openssl.h"
#include "tpm2_tool.h"
#include "config.h"

#define TPM2_ERROR_TSS2_RC_ERROR_MASK 0xFFFF

static inline UINT16 tpm2_error_get(TSS2_RC rc) {
    return ((rc & TPM2_ERROR_TSS2_RC_ERROR_MASK));
}

tool_rc tpm2_readpublic(ESYS_CONTEXT *esys_context, ESYS_TR object_handle,
        TPM2B_PUBLIC **out_public, TPM2B_NAME **name,
        TPM2B_NAME **qualified_name) {

    TSS2_RC rval = Esys_ReadPublic(esys_context, object_handle,
            ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
            out_public, name, qualified_name);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ReadPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_from_tpm_public(ESYS_CONTEXT *esys_context, TPM2_HANDLE tpm_handle,
        ESYS_TR optional_session1, ESYS_TR optional_session2,
        ESYS_TR optional_session3, ESYS_TR *object) {

    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, tpm_handle,
            optional_session1, optional_session2, optional_session3, object);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_tr_deserialize(ESYS_CONTEXT *esys_context, uint8_t const *buffer,
        size_t buffer_size, ESYS_TR *esys_handle) {

    TSS2_RC rval = Esys_TR_Deserialize(esys_context, buffer, buffer_size,
            esys_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_Deserialize, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_tr_serialize(ESYS_CONTEXT *esys_context, ESYS_TR object,
        uint8_t **buffer, size_t *buffer_size) {

    TSS2_RC rval = Esys_TR_Serialize(esys_context, object, buffer, buffer_size);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_Serialize, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_tr_get_name(ESYS_CONTEXT *esys_context, ESYS_TR handle,
        TPM2B_NAME **name) {

    TSS2_RC rval = Esys_TR_GetName(esys_context, handle, name);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_GetName, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_close(ESYS_CONTEXT *esys_context, ESYS_TR *rsrc_handle) {

    TSS2_RC rval = Esys_TR_Close(esys_context, rsrc_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_Close, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_nv_readpublic(ESYS_CONTEXT *esys_context, ESYS_TR nv_index,
        TPM2B_NV_PUBLIC **nv_public, TPM2B_NAME **nv_name) {

    TSS2_RC rval = Esys_NV_ReadPublic(esys_context, nv_index,
            ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nv_public, nv_name);

    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_ReadPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_getcap(ESYS_CONTEXT *esys_context, TPM2_CAP capability,
        UINT32 property, UINT32 property_count, TPMI_YES_NO *more_data,
        TPMS_CAPABILITY_DATA **capability_data) {

    TSS2_RC rval = Esys_GetCapability(esys_context, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
            capability, property, property_count, more_data, capability_data);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_ReadPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_nv_read(ESYS_CONTEXT *esys_context,
    tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index, UINT16 size,
    UINT16 offset, TPM2B_MAX_NV_BUFFER **data, TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

        if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_Read_Prepare(sys_context, auth_hierarchy_obj->handle,
            nv_index, size, offset);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_Read_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvread_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvread_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);
        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvread_free_name1_name2:
        Esys_Free(name2);
tpm2_nvread_free_name1:
        Esys_Free(name1);
        goto tpm2_nvread_skip_esapi_call;
    }

    rval = Esys_NV_Read(esys_context, auth_hierarchy_obj->tr_handle,
        esys_tr_nv_handle, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
        ESYS_TR_NONE, size, offset, data);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_Read, rval);
        return tool_rc_from_tpm(rval);
    }
tpm2_nvread_skip_esapi_call:
    return rc;
}

tool_rc tpm2_context_save(ESYS_CONTEXT *esys_context, ESYS_TR save_handle,
        TPMS_CONTEXT **context) {

    TSS2_RC rval = Esys_ContextSave(esys_context, save_handle, context);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_ContextSave, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_context_load(ESYS_CONTEXT *esys_context,
        const TPMS_CONTEXT *context, ESYS_TR *loaded_handle) {

    TSS2_RC rval = Esys_ContextLoad(esys_context, context, loaded_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_ContextLoad, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_flush_context(ESYS_CONTEXT *esys_context, ESYS_TR flush_handle) {

    TSS2_RC rval = Esys_FlushContext(esys_context, flush_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_FlushContext, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_start_auth_session(ESYS_CONTEXT *esys_context, ESYS_TR tpm_key,
        ESYS_TR bind, ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        const TPM2B_NONCE *nonce_caller, TPM2_SE session_type,
        const TPMT_SYM_DEF *symmetric, TPMI_ALG_HASH auth_hash,
        ESYS_TR *session_handle) {

    TSS2_RC rval = Esys_StartAuthSession(esys_context, tpm_key, bind, shandle1,
            shandle2, shandle3, nonce_caller, session_type, symmetric,
            auth_hash, session_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_StartAuthSession, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_sess_set_attributes(ESYS_CONTEXT *esys_context, ESYS_TR session,
        TPMA_SESSION flags, TPMA_SESSION mask) {

    TSS2_RC rval = Esys_TRSess_SetAttributes(esys_context, session, flags, mask);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TRSess_SetAttributes, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_sess_get_attributes(ESYS_CONTEXT *esys_context, ESYS_TR session,
        TPMA_SESSION *flags) {

    TSS2_RC rval = Esys_TRSess_GetAttributes(esys_context, session, flags);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TRSess_GetAttributes, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_sess_get_noncetpm(ESYS_CONTEXT *esys_context,
    ESYS_TR session_handle, TPM2B_NONCE **nonce_tpm) {

    TSS2_RC rval = Esys_TRSess_GetNonceTPM(esys_context, session_handle,
        nonce_tpm);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_TRSess_GetNonceTPM, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_restart(ESYS_CONTEXT *esys_context, ESYS_TR session_handle,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3) {

    TSS2_RC rval = Esys_PolicyRestart(esys_context, session_handle, shandle1,
            shandle2, shandle3);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyRestart, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_get_capability(ESYS_CONTEXT *esys_context, ESYS_TR shandle1,
        ESYS_TR shandle2, ESYS_TR shandle3, TPM2_CAP capability,
        UINT32 property, UINT32 property_count, TPMI_YES_NO *more_data,
        TPMS_CAPABILITY_DATA **capability_data) {

    TSS2_RC rval = Esys_GetCapability(esys_context, shandle1, shandle2, shandle3,
            capability, property, property_count, more_data, capability_data);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_GetCapability, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_create_primary(ESYS_CONTEXT *esys_context, ESYS_TR primary_handle,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        const TPM2B_SENSITIVE_CREATE *in_sensitive, const TPM2B_PUBLIC *in_public,
        const TPM2B_DATA *outside_info, const TPML_PCR_SELECTION *creation_pcr,
        ESYS_TR *object_handle, TPM2B_PUBLIC **out_public,
        TPM2B_CREATION_DATA **creation_data, TPM2B_DIGEST **creation_hash,
        TPMT_TK_CREATION **creation_ticket) {

    TSS2_RC rval = Esys_CreatePrimary(esys_context, primary_handle, shandle1,
            shandle2, shandle3, in_sensitive, in_public, outside_info, creation_pcr,
            object_handle, out_public, creation_data, creation_hash,
            creation_ticket);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_CreatePrimary, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_pcr_read(ESYS_CONTEXT *esys_context, ESYS_TR shandle1,
        ESYS_TR shandle2, ESYS_TR shandle3,
        const TPML_PCR_SELECTION *pcr_selection_in, UINT32 *pcr_update_counter,
        TPML_PCR_SELECTION **pcr_selection_out, TPML_DIGEST **pcr_values) {

    TSS2_RC rval = Esys_PCR_Read(esys_context, shandle1, shandle2, shandle3,
            pcr_selection_in, pcr_update_counter, pcr_selection_out, pcr_values);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PCR_Read, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_authorize(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        const TPM2B_DIGEST *approved_policy, const TPM2B_NONCE *policy_ref,
        const TPM2B_NAME *key_sign, const TPMT_TK_VERIFIED *check_ticket) {

    TSS2_RC rval = Esys_PolicyAuthorize(esys_context, policy_session, shandle1,
            shandle2, shandle3, approved_policy, policy_ref, key_sign,
            check_ticket);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyAuthorize, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_or(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        const TPML_DIGEST *p_hash_list) {

    TSS2_RC rval = Esys_PolicyOR(esys_context, policy_session, shandle1, shandle2,
            shandle3, p_hash_list);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyAuthorize, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_namehash(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
    const TPM2B_DIGEST *name_hash) {

    TSS2_RC rval = Esys_PolicyNameHash(esys_context, policy_session,
        ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, name_hash);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyNameHash, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_template(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
    const TPM2B_DIGEST *template_hash) {

    TSS2_RC rval = Esys_PolicyTemplate(esys_context, policy_session,
        ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, template_hash);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyTemplate, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_cphash(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
    const TPM2B_DIGEST *cphash) {

    TSS2_RC rval = Esys_PolicyCpHash(esys_context, policy_session,
        ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, cphash);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyCpHash, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_pcr(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        const TPM2B_DIGEST *pcr_digest, const TPML_PCR_SELECTION *pcrs) {

    TSS2_RC rval = Esys_PolicyPCR(esys_context, policy_session, shandle1,
            shandle2, shandle3, pcr_digest, pcrs);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyPCR, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_password(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3) {

    TSS2_RC rval = Esys_PolicyPassword(esys_context, policy_session, shandle1,
            shandle2, shandle3);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyPassword, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_signed(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_entity_obj, ESYS_TR policy_session,
        const TPMT_SIGNATURE *signature, INT32 expiration,
        TPM2B_TIMEOUT **timeout, TPMT_TK_AUTH **policy_ticket,
        TPM2B_NONCE *policy_qualifier, TPM2B_NONCE *nonce_tpm) {

    TSS2_RC rval = Esys_PolicySigned(esys_context, auth_entity_obj->tr_handle,
        policy_session, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, nonce_tpm,
        NULL, policy_qualifier, expiration, signature, timeout, policy_ticket);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicySigned, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_ticket(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
    const TPM2B_TIMEOUT *timeout, const TPM2B_NONCE *policyref,
    const TPM2B_NAME *authname, const TPMT_TK_AUTH *ticket) {

    TSS2_RC rval = Esys_PolicyTicket(esys_context, policy_session, ESYS_TR_NONE,
        ESYS_TR_NONE, ESYS_TR_NONE, timeout, NULL, policyref, authname, ticket);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicySigned, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_authvalue(ESYS_CONTEXT *esys_context,
        ESYS_TR policy_session, ESYS_TR shandle1, ESYS_TR shandle2,
        ESYS_TR shandle3) {

    TSS2_RC rval = Esys_PolicyAuthValue(esys_context, policy_session, shandle1,
            shandle2, shandle3);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyAuthValue, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_authorize_nv(ESYS_CONTEXT *esys_context,
    tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
    ESYS_TR policy_session, TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_index;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_index);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get auth entity obj session");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_PolicyAuthorizeNV_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index, policy_session);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_PolicyAuthorizeNV_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_policyauthorizenv_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_index, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_policyauthorizenv_free_name1_name2;
        }

        TPM2B_NAME *name3 = NULL;
        rc = tpm2_tr_get_name(esys_context, policy_session, &name3);
        if (rc != tool_rc_success) {
            goto tpm2_policyauthorizenv_free_name1_name2_name3;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, name3,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_policyauthorizenv_free_name1_name2_name3:
        Esys_Free(name3);
tpm2_policyauthorizenv_free_name1_name2:
        Esys_Free(name2);
tpm2_policyauthorizenv_free_name1:
        Esys_Free(name1);
        goto tpm2_policyauthorizenv_skip_esapi_call;
    }

    rval = Esys_PolicyAuthorizeNV(esys_context, auth_hierarchy_obj->tr_handle,
        esys_tr_nv_index, policy_session, auth_hierarchy_obj_session_handle,
        ESYS_TR_NONE, ESYS_TR_NONE);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyAuthorizeNV, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_policyauthorizenv_skip_esapi_call:
    return rc;
}

tool_rc tpm2_policy_nv(ESYS_CONTEXT *esys_context,
    tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
    ESYS_TR policy_session, const TPM2B_OPERAND *operand_b, UINT16 offset,
    TPM2_EO operation, TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_index;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_index);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_entity_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_entity_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get auth entity obj session");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_PolicyNV_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index, policy_session, operand_b,
            offset, operation);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_PolicyNV_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_policynv_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_index, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_policynv_free_name1_name2;
        }

        TPM2B_NAME *name3 = NULL;
        rc = tpm2_tr_get_name(esys_context, policy_session, &name3);
        if (rc != tool_rc_success) {
            goto tpm2_policynv_free_name1_name2_name3;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, name3,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_policynv_free_name1_name2_name3:
        Esys_Free(name3);
tpm2_policynv_free_name1_name2:
        Esys_Free(name2);
tpm2_policynv_free_name1:
        Esys_Free(name1);
        goto tpm2_policynv_skip_esapi_out;
    }

    rval = Esys_PolicyNV(esys_context, auth_hierarchy_obj->tr_handle,
        esys_tr_nv_index, policy_session, auth_entity_obj_session_handle,
        ESYS_TR_NONE, ESYS_TR_NONE, operand_b, offset, operation);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyNV, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_policynv_skip_esapi_out:
    return rc;
}

tool_rc tpm2_policy_countertimer(ESYS_CONTEXT *esys_context,
    ESYS_TR policy_session, const TPM2B_OPERAND *operand_b, UINT16 offset,
    TPM2_EO operation) {

    TSS2_RC rval = Esys_PolicyCounterTimer(esys_context, policy_session,
        ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, operand_b, offset, operation);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyCounterTimer, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_secret(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_entity_obj, ESYS_TR policy_session,
        INT32 expiration, TPMT_TK_AUTH **policy_ticket,
        TPM2B_TIMEOUT **timeout, TPM2B_NONCE *nonce_tpm,
        TPM2B_NONCE *policy_qualifier, TPM2B_DIGEST *cp_hash) {

    const TPM2B_DIGEST *cp_hash_a = NULL;

    ESYS_TR auth_entity_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_entity_obj->tr_handle, auth_entity_obj->session,
            &auth_entity_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get auth entity obj session");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_PolicySecret_Prepare(sys_context,
            auth_entity_obj->handle, policy_session, nonce_tpm, cp_hash_a,
            policy_qualifier, expiration);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_PolicySecret_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_entity_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_policysecret_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, policy_session, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_policysecret_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_entity_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_entity_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_policysecret_free_name1_name2:
        Esys_Free(name2);
tpm2_policysecret_free_name1:
        Esys_Free(name1);
        goto tpm2_policysecret_skip_esapi_call;
    }

    TSS2_RC rval = Esys_PolicySecret(esys_context, auth_entity_obj->tr_handle,
            policy_session, auth_entity_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE, nonce_tpm, cp_hash_a, policy_qualifier, expiration, timeout,
            policy_ticket);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicySecret, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_policysecret_skip_esapi_call:
    return rc;
}

tool_rc tpm2_policy_getdigest(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        TPM2B_DIGEST **policy_digest) {

    TSS2_RC rval = Esys_PolicyGetDigest(esys_context, policy_session, shandle1,
            shandle2, shandle3, policy_digest);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyGetDigest, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_command_code(ESYS_CONTEXT *esys_context,
        ESYS_TR policy_session, ESYS_TR shandle1, ESYS_TR shandle2,
        ESYS_TR shandle3, TPM2_CC code) {

    TSS2_RC rval = Esys_PolicyCommandCode(esys_context, policy_session, shandle1,
            shandle2, shandle3, code);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyCommandCode, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_nv_written(ESYS_CONTEXT *esys_context,
        ESYS_TR policy_session, ESYS_TR shandle1, ESYS_TR shandle2,
        ESYS_TR shandle3, TPMI_YES_NO written_set) {

    TSS2_RC rval = Esys_PolicyNvWritten(esys_context, policy_session, shandle1,
            shandle2, shandle3, written_set);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyNVWritten, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_locality(ESYS_CONTEXT *esys_context, ESYS_TR policy_session,
        ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
        TPMA_LOCALITY locality) {

    TSS2_RC rval = Esys_PolicyLocality(esys_context, policy_session, shandle1,
            shandle2, shandle3, locality);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyLocality, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_policy_duplication_select(ESYS_CONTEXT *esys_context,
        ESYS_TR policy_session, ESYS_TR shandle1, ESYS_TR shandle2,
        ESYS_TR shandle3, const TPM2B_NAME *object_name,
        const TPM2B_NAME *new_parent_name, TPMI_YES_NO include_object) {

    TSS2_RC rval = Esys_PolicyDuplicationSelect(esys_context, policy_session,
            shandle1, shandle2, shandle3, object_name, new_parent_name,
            include_object);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PolicyDuplicationSelect, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_mu_tpm2_handle_unmarshal(uint8_t const buffer[], size_t size,
        size_t *offset, TPM2_HANDLE *out) {

    TSS2_RC rval = Tss2_MU_TPM2_HANDLE_Unmarshal(buffer, size, offset, out);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Tss2_MU_TPM2_HANDLE_Unmarshal, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_mu_tpmt_public_marshal(TPMT_PUBLIC const *src, uint8_t buffer[],
        size_t buffer_size, size_t *offset) {

    TSS2_RC rval = Tss2_MU_TPMT_PUBLIC_Marshal(src, buffer, buffer_size,
            offset);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Tss2_MU_TPMT_PUBLIC_Marshal, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_evictcontrol(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj,
        tpm2_loaded_object *to_persist_key_obj,
        TPMI_DH_PERSISTENT persistent_handle, ESYS_TR *new_object_handle,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }


    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_EvictControl_Prepare(sys_context,
            auth_hierarchy_obj->handle, to_persist_key_obj->handle,
            persistent_handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_EvictControl_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_evictcontrol_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, to_persist_key_obj->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_evictcontrol_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_evictcontrol_free_name1_name2:
        Esys_Free(name2);
tpm2_evictcontrol_free_name1:
        Esys_Free(name1);
        goto tpm2_evictcontrol_skip_esapi_call;
    }

    TSS2_RC rval = Esys_EvictControl(esys_context, auth_hierarchy_obj->tr_handle,
            to_persist_key_obj->tr_handle, shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            persistent_handle, new_object_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_EvictControl, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_evictcontrol_skip_esapi_call:
    return rc;
}

/* This function addresses ESAPI change that changes parameter type from
 * Esys_TR to TPMI_RH_HIERARCHY and breaks backwards compatibility.
 * To keep the tools parameters consistent after v4.0 release we need to
 * map the values to appropriate type based on the version of the ESYS API.
 * Note: the mapping is based on the ESYS version recognized at compile time.
 * The TSS change can be found here:
 * https://github.com/tpm2-software/tpm2-tss/pull/1531
 */
uint32_t fix_esys_hierarchy(TPMI_RH_HIERARCHY hierarchy)
{
#if defined(ESYS_3_0)
    switch (hierarchy) {
        case TPM2_RH_NULL:
            return ESYS_TR_RH_NULL;
        case TPM2_RH_OWNER:
            return ESYS_TR_RH_OWNER;
        case TPM2_RH_ENDORSEMENT:
            return ESYS_TR_RH_ENDORSEMENT;
        case TPM2_RH_PLATFORM:
            return ESYS_TR_RH_PLATFORM;
        default:
            return TSS2_ESYS_RC_BAD_VALUE;
    }
#elif defined(ESYS_2_3)
    return hierarchy;
#else
    UNUSED(hierarchy);
    return TSS2_ESYS_RC_BAD_VALUE;
#endif
}

tool_rc tpm2_hash(ESYS_CONTEXT *esys_context, ESYS_TR shandle1, ESYS_TR shandle2,
        ESYS_TR shandle3, const TPM2B_MAX_BUFFER *data, TPMI_ALG_HASH hash_alg,
        TPMI_RH_HIERARCHY hierarchy, TPM2B_DIGEST **out_hash,
        TPMT_TK_HASHCHECK **validation) {

    TSS2_RC rval = Esys_Hash(esys_context, shandle1, shandle2, shandle3, data,
            hash_alg, fix_esys_hierarchy(hierarchy), out_hash, validation);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_Hash, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_hash_sequence_start(ESYS_CONTEXT *esys_context, const TPM2B_AUTH *auth,
        TPMI_ALG_HASH hash_alg, ESYS_TR *sequence_handle) {

    TSS2_RC rval = Esys_HashSequenceStart(esys_context, ESYS_TR_NONE, ESYS_TR_NONE,
            ESYS_TR_NONE, auth, hash_alg, sequence_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_HashSequenceStart, rval);
        return tool_rc_from_tpm(rval);
    }

    return tpm2_tr_set_auth(esys_context, *sequence_handle, auth);
}

tool_rc tpm2_sequence_update(ESYS_CONTEXT *esys_context, ESYS_TR sequence_handle,
        const TPM2B_MAX_BUFFER *buffer) {

    TSS2_RC rval = Esys_SequenceUpdate(esys_context, sequence_handle, ESYS_TR_PASSWORD,
            ESYS_TR_NONE, ESYS_TR_NONE, buffer);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_SequenceUpdate, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_sequence_complete(ESYS_CONTEXT *esys_context,
        ESYS_TR sequence_handle, const TPM2B_MAX_BUFFER *buffer,
        TPMI_RH_HIERARCHY hierarchy, TPM2B_DIGEST **result,
        TPMT_TK_HASHCHECK **validation) {

    TSS2_RC rval = Esys_SequenceComplete(esys_context, sequence_handle,
            ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, buffer,
            fix_esys_hierarchy(hierarchy), result, validation);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_SequenceComplete, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_event_sequence_complete(ESYS_CONTEXT *ectx, ESYS_TR pcr,
        ESYS_TR sequence_handle, tpm2_session *session,
        const TPM2B_MAX_BUFFER *buffer, TPML_DIGEST_VALUES **results) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx, pcr, session,
            &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    TSS2_RC rval = Esys_EventSequenceComplete(ectx, pcr, sequence_handle, shandle1,
            ESYS_TR_PASSWORD, ESYS_TR_NONE, buffer, results);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_EventSequenceComplete, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_tr_set_auth(ESYS_CONTEXT *esys_context, ESYS_TR handle,
        TPM2B_AUTH const *auth_value) {

    TSS2_RC rval = Esys_TR_SetAuth(esys_context, handle, auth_value);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_SequenceComplete, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_activatecredential(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *activatehandleobj, tpm2_loaded_object *keyhandleobj,
        const TPM2B_ID_OBJECT *credential_blob,
        const TPM2B_ENCRYPTED_SECRET *secret, TPM2B_DIGEST **cert_info,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR keyobj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            keyhandleobj->tr_handle, keyhandleobj->session,
            &keyobj_session_handle); //shandle1
    if (rc != tool_rc_success) {
        return rc;
    }

    ESYS_TR activateobj_session_handle = ESYS_TR_NONE;
    rc = tpm2_auth_util_get_shandle(esys_context, activatehandleobj->tr_handle,
            activatehandleobj->session, &activateobj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_ActivateCredential_Prepare(sys_context,
            activatehandleobj->handle, keyhandleobj->handle, credential_blob,
            secret);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_ActivateCredential_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, activatehandleobj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_activatecredential_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, keyhandleobj->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_activatecredential_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(activatehandleobj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(activatehandleobj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_activatecredential_free_name1_name2:
        Esys_Free(name2);
tpm2_activatecredential_free_name1:
        Esys_Free(name1);
        goto tpm2_activatecredential_skip_esapi_call;
    }

    TSS2_RC rval = Esys_ActivateCredential(esys_context,
            activatehandleobj->tr_handle, keyhandleobj->tr_handle,
            activateobj_session_handle, keyobj_session_handle, ESYS_TR_NONE,
            credential_blob, secret, cert_info);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ActivateCredential, rval);
        rc = tool_rc_from_tpm(rval);
        return rc;
    }

tpm2_activatecredential_skip_esapi_call:
    return rc;
}

tool_rc tpm2_create(ESYS_CONTEXT *esys_context, tpm2_loaded_object *parent_obj,
        const TPM2B_SENSITIVE_CREATE *in_sensitive, const TPM2B_PUBLIC *in_public,
        const TPM2B_DATA *outside_info, const TPML_PCR_SELECTION *creation_pcr,
        TPM2B_PRIVATE **out_private, TPM2B_PUBLIC **out_public,
        TPM2B_CREATION_DATA **creation_data, TPM2B_DIGEST **creation_hash,
        TPMT_TK_CREATION **creation_ticket, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, parent_obj->tr_handle,
            parent_obj->session, &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Create_Prepare(sys_context, parent_obj->handle,
            in_sensitive, in_public, outside_info, creation_pcr);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Create_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, parent_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_create_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(parent_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(parent_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_create_free_name1:
        Esys_Free(name1);
        goto tpm2_create_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Create(esys_context, parent_obj->tr_handle, shandle1,
            ESYS_TR_NONE, ESYS_TR_NONE, in_sensitive, in_public, outside_info,
            creation_pcr, out_private, out_public, creation_data, creation_hash,
            creation_ticket);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_Create, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_create_skip_esapi_call:
    return rc;
}

tool_rc tpm2_create_loaded(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *parent_obj,
        const TPM2B_SENSITIVE_CREATE *in_sensitive,
        const TPM2B_TEMPLATE *in_public, ESYS_TR *object_handle,
        TPM2B_PRIVATE **out_private, TPM2B_PUBLIC **out_public,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, parent_obj->tr_handle,
            parent_obj->session, &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_CreateLoaded_Prepare(sys_context,
            parent_obj->handle, in_sensitive, in_public);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_CreateLoaded_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, parent_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_createloaded_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(parent_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(parent_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_createloaded_free_name1:
        Esys_Free(name1);
        goto tpm2_createloaded_skip_esapi_call;
    }

    TSS2_RC rval = Esys_CreateLoaded(esys_context, parent_obj->tr_handle,
            shandle1, ESYS_TR_NONE, ESYS_TR_NONE, in_sensitive, in_public,
            object_handle, out_private, out_public);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_CreateLoaded, rval);
        return tool_rc_from_tpm(rval);
    }
tpm2_createloaded_skip_esapi_call:
    return rc;
}

tool_rc tpm2_object_change_auth(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *parent_object, tpm2_loaded_object *object,
        const TPM2B_AUTH *new_auth, TPM2B_PRIVATE **out_private,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, object->tr_handle,
            object->session, &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_ObjectChangeAuth_Prepare(sys_context,
            object->handle, parent_object->handle, new_auth);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_ObjectChangeAuth_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, object->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_objectchangeauth_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, parent_object->tr_handle,
            &name2);
        if (rc != tool_rc_success) {
            goto tpm2_objectchangeauth_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(object->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(object->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_objectchangeauth_free_name1_name2:
        Esys_Free(name2);
tpm2_objectchangeauth_free_name1:
        Esys_Free(name1);
        goto tpm2_objectchangeauth_skip_esapi_call;
    }

    TSS2_RC rval = Esys_ObjectChangeAuth(esys_context, object->tr_handle,
            parent_object->tr_handle, shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            new_auth, out_private);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ObjectChangeAuth, rval);
        return tool_rc_from_tpm(rval);
    }
tpm2_objectchangeauth_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nv_change_auth(ESYS_CONTEXT *esys_context, tpm2_loaded_object *nv,
        const TPM2B_AUTH *new_auth, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, nv->tr_handle,
            nv->session, &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_NV_ChangeAuth_Prepare(sys_context, nv->handle,
            new_auth);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_ChangeAuth_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, nv->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvchangeauth_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(nv->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(nv->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvchangeauth_free_name1:
        Esys_Free(name1);
        goto tpm2_nvchangeauth_skip_esapi_call;
    }

    TSS2_RC rval = Esys_NV_ChangeAuth(esys_context, nv->tr_handle, shandle1,
            ESYS_TR_NONE, ESYS_TR_NONE, new_auth);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_ChangeAuth, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvchangeauth_skip_esapi_call:
    return rc;
}

tool_rc tpm2_hierarchy_change_auth(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *hierarchy, const TPM2B_AUTH *new_auth,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, hierarchy->tr_handle,
            hierarchy->session, &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_HierarchyChangeAuth_Prepare(sys_context,
            hierarchy->handle, new_auth);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_HierarchyChangeAuth_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, hierarchy->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_hierarchychangeauth_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(hierarchy->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(hierarchy->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_hierarchychangeauth_free_name1:
        Esys_Free(name1);
        goto tpm2_hierarchychangeauth_skip_esapi_call;
    }

    TSS2_RC rval = Esys_HierarchyChangeAuth(esys_context, hierarchy->tr_handle,
            shandle1, ESYS_TR_NONE, ESYS_TR_NONE, new_auth);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_HierarchyChangeAuth, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_hierarchychangeauth_skip_esapi_call:
    return rc;
}

tool_rc tpm2_certify(ESYS_CONTEXT *ectx, tpm2_loaded_object *certifiedkey_obj,
        tpm2_loaded_object *signingkey_obj, TPM2B_DATA *qualifying_data,
        TPMT_SIG_SCHEME *scheme, TPM2B_ATTEST **certify_info,
        TPMT_SIGNATURE **signature, TPM2B_DIGEST *cp_hash) {

    ESYS_TR certifiedkey_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx, certifiedkey_obj->tr_handle,
            certifiedkey_obj->session, &certifiedkey_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get session handle for TPM object");
        return rc;
    }

    ESYS_TR signingkey_session_handle = ESYS_TR_NONE;
    rc = tpm2_auth_util_get_shandle(ectx, signingkey_obj->tr_handle,
            signingkey_obj->session, &signingkey_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get session handle for key");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(ectx, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Certify_Prepare(sys_context,
        certifiedkey_obj->handle, signingkey_obj->handle, qualifying_data, scheme);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Certify_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(ectx, certifiedkey_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_certify_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(ectx, signingkey_obj->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_certify_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(certifiedkey_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(certifiedkey_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_certify_free_name1_name2:
        Esys_Free(name2);
tpm2_certify_free_name1:
        Esys_Free(name1);
        goto tpm2_certify_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Certify(ectx, certifiedkey_obj->tr_handle,
            signingkey_obj->tr_handle, certifiedkey_session_handle,
            signingkey_session_handle, ESYS_TR_NONE, qualifying_data, scheme,
            certify_info, signature);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Eys_Certify, rval);
        rc = tool_rc_from_tpm(rval);
        return rc;
    }

tpm2_certify_skip_esapi_call:
    return rc;
}

tool_rc tpm2_rsa_decrypt(ESYS_CONTEXT *ectx, tpm2_loaded_object *keyobj,
        const TPM2B_PUBLIC_KEY_RSA *cipher_text, const TPMT_RSA_DECRYPT *scheme,
        const TPM2B_DATA *label, TPM2B_PUBLIC_KEY_RSA **message,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR keyobj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx, keyobj->tr_handle,
            keyobj->session, &keyobj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(ectx, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_RSA_Decrypt_Prepare(sys_context, keyobj->handle,
        cipher_text, scheme, label);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_RSA_Decrypt_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(ectx, keyobj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_rsadecrypt_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(keyobj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(keyobj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_rsadecrypt_free_name1:
        Esys_Free(name1);
        goto tpm2_rsadecrypt_skip_esapi_call;
    }

    TSS2_RC rval = Esys_RSA_Decrypt(ectx, keyobj->tr_handle,
            keyobj_session_handle, ESYS_TR_NONE, ESYS_TR_NONE, cipher_text,
            scheme, label, message);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_RSA_Decrypt, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_rsadecrypt_skip_esapi_call:
    return rc;
}

tool_rc tpm2_rsa_encrypt(ESYS_CONTEXT *ectx, tpm2_loaded_object *keyobj,
        const TPM2B_PUBLIC_KEY_RSA *message, const TPMT_RSA_DECRYPT *scheme,
        const TPM2B_DATA *label, TPM2B_PUBLIC_KEY_RSA **cipher_text) {

    TSS2_RC rval = Esys_RSA_Encrypt(ectx, keyobj->tr_handle,
            ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, message, scheme,
            label, cipher_text);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_RSA_Encrypt, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_load(ESYS_CONTEXT *esys_context, tpm2_loaded_object *parentobj,
        const TPM2B_PRIVATE *in_private, const TPM2B_PUBLIC *in_public,
        ESYS_TR *object_handle, TPM2B_DIGEST *cp_hash) {

    ESYS_TR parent_object_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, parentobj->tr_handle,
            parentobj->session, &parent_object_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get parent object session handle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Load_Prepare(sys_context, parentobj->handle,
            in_private, in_public);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Load_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, parentobj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_load_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(parentobj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(parentobj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_load_free_name1:
        Esys_Free(name1);
        goto tpm2_load_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Load(esys_context, parentobj->tr_handle,
            parent_object_session_handle, ESYS_TR_NONE, ESYS_TR_NONE, in_private,
            in_public, object_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Eys_Load, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_load_skip_esapi_call:
    return rc;
}

tool_rc tpm2_clear(ESYS_CONTEXT *esys_context,
tpm2_loaded_object *auth_hierarchy, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy->tr_handle, auth_hierarchy->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle for hierarchy");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Clear_Prepare(sys_context,
            auth_hierarchy->handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Clear_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_clear_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_clear_free_name1:
        Esys_Free(name1);
        goto tpm2_clear_skip_esapi_call;
    }


    TSS2_RC rval = Esys_Clear(esys_context, auth_hierarchy->tr_handle, shandle1,
            ESYS_TR_NONE, ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS && rval != TPM2_RC_INITIALIZE) {
        LOG_PERR(Esys_Clear, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_clear_skip_esapi_call:
    return rc;
}

tool_rc tpm2_clearcontrol(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy, TPMI_YES_NO disable_clear,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy->tr_handle, auth_hierarchy->session, &shandle);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_ClearControl_Prepare(sys_context,
            auth_hierarchy->handle, disable_clear);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_ClearControl_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_clearcontrol_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_clearcontrol_free_name1:
        Esys_Free(name1);
        goto tpm2_clearcontrol_skip_esapi_call;
    }

    TSS2_RC rval = Esys_ClearControl(esys_context, auth_hierarchy->tr_handle,
            shandle, ESYS_TR_NONE, ESYS_TR_NONE, disable_clear);
    if (rval != TPM2_RC_SUCCESS && rval != TPM2_RC_INITIALIZE) {
        LOG_PERR(Esys_ClearControl, rval);
        return tool_rc_from_tpm(rval);
    }
tpm2_clearcontrol_skip_esapi_call:
    return rc;
}

tool_rc tpm2_dictionarylockout_setup(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy, UINT32 max_tries,
        UINT32 recovery_time, UINT32 lockout_recovery_time,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy->tr_handle, auth_hierarchy->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for lockout hierarchy");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_DictionaryAttackParameters_Prepare(sys_context,
            auth_hierarchy->handle, max_tries, recovery_time,
            lockout_recovery_time);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_DictionaryAttackParameters_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_dictionary_parameters_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_dictionary_parameters_free_name1:
        Esys_Free(name1);
        goto tpm2_dictionary_parameters_skip_esapi_call;
    }

    LOG_INFO("Setting up Dictionary Lockout parameters.");
    TPM2_RC rval = Esys_DictionaryAttackParameters(esys_context,
            auth_hierarchy->tr_handle, shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            max_tries, recovery_time, lockout_recovery_time);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_DictionaryAttackParameters, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_dictionary_parameters_skip_esapi_call:
    return rc;
}

tool_rc tpm2_dictionarylockout_reset(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy->tr_handle, auth_hierarchy->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for lockout hierarchy");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_DictionaryAttackLockReset_Prepare(sys_context,
            auth_hierarchy->handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_DictionaryAttackLockReset_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_dictionary_attack_reset_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_dictionary_attack_reset_free_name1:
        Esys_Free(name1);
        goto tpm2_dictionary_attack_reset_skip_esapi_call;
    }

    LOG_INFO("Resetting dictionary lockout state.");
    TPM2_RC rval = Esys_DictionaryAttackLockReset(esys_context,
            auth_hierarchy->tr_handle, shandle1, ESYS_TR_NONE,
            ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_DictionaryAttackLockReset, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_dictionary_attack_reset_skip_esapi_call:
    return rc;
}

tool_rc tpm2_duplicate(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *duplicable_key, tpm2_loaded_object *new_parent,
        const TPM2B_DATA *in_key, const TPMT_SYM_DEF_OBJECT *sym_alg,
        TPM2B_DATA **out_key, TPM2B_PRIVATE **duplicate,
        TPM2B_ENCRYPTED_SECRET **encrypted_seed, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            duplicable_key->tr_handle, duplicable_key->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Duplicate_Prepare(sys_context,
            duplicable_key->handle, new_parent->handle, in_key, sym_alg);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Duplicate_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, duplicable_key->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_duplicate_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, new_parent->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_duplicate_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(duplicable_key->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(duplicable_key->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_duplicate_free_name1_name2:
        Esys_Free(name2);
tpm2_duplicate_free_name1:
        Esys_Free(name1);
        goto tpm2_duplicate_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Duplicate(esys_context, duplicable_key->tr_handle,
            new_parent->tr_handle, shandle1, ESYS_TR_NONE, ESYS_TR_NONE, in_key,
            sym_alg, out_key, duplicate, encrypted_seed);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_Duplicate, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_duplicate_skip_esapi_call:
    return rc;
}

tool_rc tpm2_encryptdecrypt(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *encryption_key_obj, TPMI_YES_NO decrypt,
        TPMI_ALG_SYM_MODE mode, const TPM2B_IV *iv_in,
        const TPM2B_MAX_BUFFER *input_data, TPM2B_MAX_BUFFER **output_data,
        TPM2B_IV **iv_out, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
    encryption_key_obj->tr_handle, encryption_key_obj->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    /* Keep track of which version you ran for error reporting.*/
    unsigned version = 2;

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_EncryptDecrypt2_Prepare(sys_context,
        encryption_key_obj->handle, input_data, decrypt, mode, iv_in);
        if (tpm2_error_get(rval) == TPM2_RC_COMMAND_CODE) {
            version = 1;
            rval = Tss2_Sys_EncryptDecrypt_Prepare(sys_context,
            encryption_key_obj->handle, decrypt, mode, iv_in, input_data);
        }

        if (rval != TPM2_RC_SUCCESS) {
            if (version == 2) {
                LOG_PERR(Tss2_Sys_EncryptDecrypt2_Prepare, rval);
            } else {
                LOG_PERR(Tss2_Sys_EncryptDecrypt_Prepare, rval);
            }
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, encryption_key_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_encryptdecrypt_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(encryption_key_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(encryption_key_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_encryptdecrypt_free_name1:
        Esys_Free(name1);
        goto tpm2_encryptdecrypt_skip_esapi_call;
    }

    TSS2_RC rval = Esys_EncryptDecrypt2(esys_context,
            encryption_key_obj->tr_handle, shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            input_data, decrypt, mode, iv_in, output_data, iv_out);
    if (tpm2_error_get(rval) == TPM2_RC_COMMAND_CODE) {
        version = 1;
        rval = Esys_EncryptDecrypt(esys_context, encryption_key_obj->tr_handle,
                shandle1, ESYS_TR_NONE, ESYS_TR_NONE, decrypt, mode, iv_in,
                input_data, output_data, iv_out);
    }

    if (rval != TPM2_RC_SUCCESS) {
        if (version == 2) {
            LOG_PERR(Esys_EncryptDecrypt2, rval);
        } else {
            LOG_PERR(Esys_EncryptDecrypt, rval);
        }
        return tool_rc_from_tpm(rval);
    }

tpm2_encryptdecrypt_skip_esapi_call:
    return rc;
}

tool_rc tpm2_hierarchycontrol(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy, TPMI_RH_ENABLES enable,
        TPMI_YES_NO state, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy->tr_handle, auth_hierarchy->session, &shandle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle for hierarchy");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_HierarchyControl_Prepare(sys_context,
        auth_hierarchy->handle, enable, state);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_HierarchyControl_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_hierarchycontrol_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_hierarchycontrol_free_name1:
        Esys_Free(name1);
        goto tpm2_hierarchycontrol_skip_esapi_call;
    }


    TSS2_RC rval = Esys_HierarchyControl(esys_context, auth_hierarchy->tr_handle,
            shandle, ESYS_TR_NONE, ESYS_TR_NONE, enable, state);
    if (rval != TPM2_RC_SUCCESS && rval != TPM2_RC_INITIALIZE) {
        LOG_PERR(Esys_HierarchyControl, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_hierarchycontrol_skip_esapi_call:
    return rc;
}

tool_rc tpm2_hmac(ESYS_CONTEXT *esys_context, tpm2_loaded_object *hmac_key_obj,
        TPMI_ALG_HASH halg, const TPM2B_MAX_BUFFER *input_buffer,
        TPM2B_DIGEST **out_hmac, TPM2B_DIGEST *cp_hash) {

    ESYS_TR hmac_key_obj_shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            hmac_key_obj->tr_handle, hmac_key_obj->session,
            &hmac_key_obj_shandle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get hmac_key_obj_shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_HMAC_Prepare(sys_context, hmac_key_obj->handle,
        input_buffer, halg);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_HMAC_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, hmac_key_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_hmac_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(hmac_key_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(hmac_key_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_hmac_free_name1:
        Esys_Free(name1);
        goto tpm2_hmac_skip_esapi_call;
    }

    TPM2_RC rval = Esys_HMAC(esys_context, hmac_key_obj->tr_handle,
            hmac_key_obj_shandle, ESYS_TR_NONE, ESYS_TR_NONE, input_buffer,
            halg, out_hmac);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_HMAC, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_hmac_skip_esapi_call:
    return rc;
}

tool_rc tpm2_hmac_start(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *hmac_key_obj, TPMI_ALG_HASH halg,
        ESYS_TR *sequence_handle) {

    ESYS_TR hmac_key_obj_shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            hmac_key_obj->tr_handle, hmac_key_obj->session,
            &hmac_key_obj_shandle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get hmac_key_obj_shandle");
        return rc;
    }

    TPM2B_AUTH null_auth = { .size = 0 };
    TPM2_RC rval = Esys_HMAC_Start(esys_context, hmac_key_obj->tr_handle,
            hmac_key_obj_shandle, ESYS_TR_NONE, ESYS_TR_NONE, &null_auth, halg,
            sequence_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_HMAC, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_hmac_sequenceupdate(ESYS_CONTEXT *esys_context,
        ESYS_TR sequence_handle, tpm2_loaded_object *hmac_key_obj,
        const TPM2B_MAX_BUFFER *input_buffer) {

    ESYS_TR hmac_key_obj_shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            hmac_key_obj->tr_handle, hmac_key_obj->session,
            &hmac_key_obj_shandle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get hmac_key_obj_shandle");
        return rc;
    }

    TPM2_RC rval = Esys_SequenceUpdate(esys_context, sequence_handle,
            hmac_key_obj_shandle, ESYS_TR_NONE, ESYS_TR_NONE, input_buffer);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_HMAC, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_hmac_sequencecomplete(ESYS_CONTEXT *esys_context,
        ESYS_TR sequence_handle, tpm2_loaded_object *hmac_key_obj,
        const TPM2B_MAX_BUFFER *input_buffer, TPM2B_DIGEST **result,
        TPMT_TK_HASHCHECK **validation) {

    ESYS_TR hmac_key_obj_shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            hmac_key_obj->tr_handle, hmac_key_obj->session,
            &hmac_key_obj_shandle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get hmac_key_obj_shandle");
        return rc;
    }

    TPM2_RC rval = Esys_SequenceComplete(esys_context, sequence_handle,
            hmac_key_obj_shandle, ESYS_TR_NONE, ESYS_TR_NONE, input_buffer,
            TPM2_RH_NULL, result, validation);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_HMAC, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_import(ESYS_CONTEXT *esys_context, tpm2_loaded_object *parent_obj,
        const TPM2B_DATA *encryption_key, const TPM2B_PUBLIC *object_public,
        const TPM2B_PRIVATE *duplicate, const TPM2B_ENCRYPTED_SECRET *in_sym_seed,
        const TPMT_SYM_DEF_OBJECT *symmetric_alg, TPM2B_PRIVATE **out_private,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR parentobj_shandle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, parent_obj->tr_handle,
            parent_obj->session, &parentobj_shandle);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for phandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Import_Prepare(sys_context,
            parent_obj->handle, encryption_key, object_public, duplicate,
            in_sym_seed, symmetric_alg);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Import_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, parent_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_import_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(parent_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(parent_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_import_free_name1:
        Esys_Free(name1);
        goto tpm2_import_skip_esapi_call;
    }

    TPM2_RC rval = Esys_Import(esys_context, parent_obj->tr_handle,
            parentobj_shandle, ESYS_TR_NONE, ESYS_TR_NONE, encryption_key,
            object_public, duplicate, in_sym_seed, symmetric_alg, out_private);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_HMAC, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_import_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nv_definespace(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, const TPM2B_AUTH *auth,
        const TPM2B_NV_PUBLIC *public_info, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    ESYS_TR nvHandle;

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_NV_DefineSpace_Prepare(sys_context,
            auth_hierarchy_obj->handle, auth, public_info);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_DefineSpace_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvdefinespace_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvdefinespace_free_name1:
        Esys_Free(name1);
        goto tpm2_nvdefinespace_skip_esapi_call;
    }

    TSS2_RC rval = Esys_NV_DefineSpace(esys_context,
            auth_hierarchy_obj->tr_handle, shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            auth, public_info, &nvHandle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_ERR("Failed to define NV area at index 0x%X",
                public_info->nvPublic.nvIndex);
        LOG_PERR(Esys_NV_DefineSpace, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvdefinespace_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nv_increment(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    // Convert TPM2_HANDLE ctx.nv_index to an ESYS_TR
    ESYS_TR esys_tr_nv_index;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_index);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_Increment_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_Increment_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvincrement_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_index, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvincrement_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvincrement_free_name1_name2:
        Esys_Free(name2);
tpm2_nvincrement_free_name1:
        Esys_Free(name1);
        goto tpm2_nvincrement_skip_esapi_call;
    }

    rval = Esys_NV_Increment(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_index, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        return tool_rc_from_tpm(rval);
    }

tpm2_nvincrement_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvreadlock(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_ReadLock_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_ReadLock_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_readlock_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvreadlock_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvreadlock_free_name1_name2:
        Esys_Free(name2);
tpm2_readlock_free_name1:
        Esys_Free(name1);
        goto tpm2_nvreadlock_skip_esapi_call;
    }

    rval = Esys_NV_ReadLock(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_handle, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_ReadLock, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvreadlock_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvwritelock(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_WriteLock_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_WriteLock_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvwritelock_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvwritelock_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvwritelock_free_name1_name2:
        Esys_Free(name2);
tpm2_nvwritelock_free_name1:
        Esys_Free(name1);
        goto tpm2_nvwritelock_skip_esapi_call;
    }


    rval = Esys_NV_WriteLock(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_handle, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_WriteLock, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvwritelock_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvglobalwritelock(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2B_DIGEST *cp_hash) {

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_NV_GlobalWriteLock_Prepare(sys_context,
            auth_hierarchy_obj->handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_GlobalWriteLock_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_globalnvwritelock_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_globalnvwritelock_free_name1:
        Esys_Free(name1);
        goto tpm2_globalnvwritelock_skip_esapi_call;
    }

    TSS2_RC rval = Esys_NV_GlobalWriteLock(esys_context, auth_hierarchy_obj->tr_handle,
            auth_hierarchy_obj_session_handle, ESYS_TR_NONE, ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_GlobalWriteLock, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_globalnvwritelock_skip_esapi_call:
    return rc;
}

tool_rc tpm2_tr_from_tpm_public(ESYS_CONTEXT *esys_context, TPM2_HANDLE handle, ESYS_TR *tr_handle) {

    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, handle, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, tr_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_nvsetbits(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        UINT64 bits, TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
        ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
        auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
        &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_SetBits_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index, bits);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_SetBits_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvsetbits_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvsetbits_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvsetbits_free_name1_name2:
        Esys_Free(name2);
tpm2_nvsetbits_free_name1:
        Esys_Free(name1);
        goto tpm2_nvsetbits_skip_esapi_call;
    }

    rval = Esys_NV_SetBits(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_handle, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE, bits);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_SetBits, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvsetbits_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvextend(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        TPM2B_MAX_NV_BUFFER *data, TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_Extend_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index, data);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_Extend_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvextend_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvextend_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvextend_free_name1_name2:
        Esys_Free(name2);
tpm2_nvextend_free_name1:
        Esys_Free(name1);
        goto tpm2_nvextend_skip_esapi_call;
    }

    rval = Esys_NV_Extend(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_handle, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE, data);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_Extend, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvextend_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvundefine(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_UndefineSpace_Prepare(sys_context,
            auth_hierarchy_obj->handle, nv_index);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_UndefineSpace_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvundefine_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvundefine_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvundefine_free_name1_name2:
        Esys_Free(name2);
tpm2_nvundefine_free_name1:
        Esys_Free(name1);
        goto tpm2_nvundefine_skip_esapi_call;
    }

    rval = Esys_NV_UndefineSpace(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_handle, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_ERR("Failed to release NV area at index 0x%X", nv_index);
        LOG_PERR(Esys_NV_UndefineSpace, rval);
        return tool_rc_from_tpm(rval);
    }

    LOG_INFO("Success to release NV area at index 0x%x.", nv_index);

tpm2_nvundefine_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvundefinespecial(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nv_index,
        tpm2_session *policy_session, TPM2B_DIGEST *cp_hash) {

    ESYS_TR esys_tr_nv_handle;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_handle);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle");
        return rc;
    }

    ESYS_TR policy_session_handle = tpm2_session_get_handle(policy_session);

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_UndefineSpaceSpecial_Prepare(sys_context, nv_index,
            auth_hierarchy_obj->handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_UndefineSpaceSpecial_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvundefinespecial_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle,
            &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvundefinespecial_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvundefinespecial_free_name1_name2:
        Esys_Free(name2);
tpm2_nvundefinespecial_free_name1:
        Esys_Free(name1);
        goto tpm2_nvundefinespecial_skip_esapi_call;
    }

    rval = Esys_NV_UndefineSpaceSpecial(esys_context,
            esys_tr_nv_handle,
            auth_hierarchy_obj->tr_handle,
            policy_session_handle, // policy session
            auth_hierarchy_obj_session_handle, // auth session for hierarchy
            ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_ERR("Failed to release NV area at index 0x%X", nv_index);
        LOG_PERR(Esys_NV_UndefineSpaceSpecial, rval);
        return tool_rc_from_tpm(rval);
    }

    LOG_INFO("Success to release NV area at index 0x%x.", nv_index);

tpm2_nvundefinespecial_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvwrite(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj, TPM2_HANDLE nvindex,
        const TPM2B_MAX_NV_BUFFER *data, UINT16 offset, TPM2B_DIGEST *cp_hash) {

    // Convert TPM2_HANDLE ctx.nv_index to an ESYS_TR
    ESYS_TR esys_tr_nv_index;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nvindex, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_index);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            auth_hierarchy_obj->tr_handle, auth_hierarchy_obj->session,
            &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_Write_Prepare(sys_context, auth_hierarchy_obj->handle,
            nvindex, data, offset);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_Write_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, auth_hierarchy_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvwrite_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_index, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvwrite_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(auth_hierarchy_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(auth_hierarchy_obj->session), cp_hash);
        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvwrite_free_name1_name2:
        Esys_Free(name2);
tpm2_nvwrite_free_name1:
        Esys_Free(name1);
        goto tpm2_nvwrite_skip_esapi_call;
    }

    rval = Esys_NV_Write(esys_context, auth_hierarchy_obj->tr_handle,
            esys_tr_nv_index, auth_hierarchy_obj_session_handle, ESYS_TR_NONE,
            ESYS_TR_NONE, data, offset);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_ERR("Failed to write NV area at index 0x%X", nvindex);
        LOG_PERR(Tss2_Sys_NV_Write, rval);
        return tool_rc_from_tpm(rval);
    }

    LOG_INFO("Success to write NV area at index 0x%x offset 0x%x.", nvindex,
            offset);

tpm2_nvwrite_skip_esapi_call:
    return rc;
}

tool_rc tpm2_pcr_allocate(ESYS_CONTEXT *esys_context,
        tpm2_loaded_object *auth_hierarchy_obj,
        const TPML_PCR_SELECTION *pcr_allocation) {

    TSS2_RC rval;
    TPMI_YES_NO allocation_success;
    UINT32 max_pcr;
    UINT32 size_needed;
    UINT32 size_available;

    ESYS_TR auth_hierarchy_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, ESYS_TR_RH_PLATFORM,
            auth_hierarchy_obj->session, &auth_hierarchy_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for lockout hierarchy");
        return rc;
    }

    rval = Esys_PCR_Allocate(esys_context, ESYS_TR_RH_PLATFORM,
            auth_hierarchy_obj_session_handle, ESYS_TR_NONE, ESYS_TR_NONE,
            pcr_allocation, &allocation_success, &max_pcr, &size_needed,
            &size_available);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_ERR("Could not allocate PCRs.");
        LOG_PERR(Esys_PCR_Allocate, rval);
        return tool_rc_from_tpm(rval);
    }

    if (!allocation_success) {
        LOG_ERR("Allocation failed. "
                "MaxPCR: %i, Size Needed: %i, Size available: %i", max_pcr,
                size_needed, size_available);
        return tool_rc_general_error;
    }

    return tool_rc_success;
}

tool_rc tpm2_sign(ESYS_CONTEXT *esys_context, tpm2_loaded_object *signingkey_obj,
        TPM2B_DIGEST *digest, TPMT_SIG_SCHEME *in_scheme,
        TPMT_TK_HASHCHECK *validation, TPMT_SIGNATURE **signature,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR signingkey_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            signingkey_obj->tr_handle, signingkey_obj->session,
            &signingkey_obj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Sign_Prepare(sys_context,
        signingkey_obj->handle, digest, in_scheme, validation);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Sign_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, signingkey_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_sign_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(signingkey_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(signingkey_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_sign_free_name1:
        Esys_Free(name1);
        goto tpm2_sign_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Sign(esys_context, signingkey_obj->tr_handle,
            signingkey_obj_session_handle, ESYS_TR_NONE, ESYS_TR_NONE, digest,
            in_scheme, validation, signature);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Eys_Sign, rval);
        rc = tool_rc_from_tpm(rval);
        return rc;
    }
tpm2_sign_skip_esapi_call:
    return rc;
}

tool_rc tpm2_nvcertify(ESYS_CONTEXT *esys_context,
    tpm2_loaded_object *signingkey_obj, tpm2_loaded_object *nvindex_authobj,
    TPM2_HANDLE nv_index, UINT16 offset, UINT16 size,
    TPMT_SIG_SCHEME *in_scheme, TPM2B_ATTEST **certify_info,
    TPMT_SIGNATURE **signature, TPM2B_DATA *policy_qualifier,
    TPM2B_DIGEST *cp_hash) {

    ESYS_TR signingkey_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            signingkey_obj->tr_handle, signingkey_obj->session,
            &signingkey_obj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    ESYS_TR nvindex_authobj_session_handle = ESYS_TR_NONE;
    rc = tpm2_auth_util_get_shandle(esys_context,
        nvindex_authobj->tr_handle, nvindex_authobj->session,
        &nvindex_authobj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    ESYS_TR esys_tr_nv_index;
    TSS2_RC rval = Esys_TR_FromTPMPublic(esys_context, nv_index, ESYS_TR_NONE,
            ESYS_TR_NONE, ESYS_TR_NONE, &esys_tr_nv_index);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_TR_FromTPMPublic, rval);
        return tool_rc_from_tpm(rval);
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        rval = Tss2_Sys_NV_Certify_Prepare(sys_context, signingkey_obj->handle,
            nvindex_authobj->handle, nv_index, policy_qualifier, in_scheme,
            size, offset);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_NV_Certify_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, signingkey_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_nvcertify_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, nvindex_authobj->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_nvcertify_free_name1_name2;
        }

        TPM2B_NAME *name3 = NULL;
        rc = tpm2_tr_get_name(esys_context, esys_tr_nv_index, &name3);
        if (rc != tool_rc_success) {
            goto tpm2_nvcertify_free_name1_name2_name3;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(signingkey_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, name3,
            tpm2_session_get_authhash(signingkey_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_nvcertify_free_name1_name2_name3:
        Esys_Free(name3);
tpm2_nvcertify_free_name1_name2:
        Esys_Free(name2);
tpm2_nvcertify_free_name1:
        Esys_Free(name1);
        goto tpm2_nvcertify_skip_esapi_call;
    }

    rval = Esys_NV_Certify(esys_context, signingkey_obj->tr_handle,
        nvindex_authobj->tr_handle, esys_tr_nv_index,
        signingkey_obj_session_handle, nvindex_authobj_session_handle,
        ESYS_TR_NONE, policy_qualifier, in_scheme, size, offset, certify_info,
        signature);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_NV_Certify, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_nvcertify_skip_esapi_call:
    return rc;
}

tool_rc tpm2_certifycreation(ESYS_CONTEXT *esys_context,
    tpm2_loaded_object *signingkey_obj, tpm2_loaded_object *certifiedkey_obj,
    TPM2B_DIGEST *creation_hash, TPMT_SIG_SCHEME *in_scheme,
    TPMT_TK_CREATION *creation_ticket, TPM2B_ATTEST **certify_info,
    TPMT_SIGNATURE **signature, TPM2B_DATA *policy_qualifier,
    TPM2B_DIGEST *cp_hash) {

    ESYS_TR signingkey_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            signingkey_obj->tr_handle, signingkey_obj->session,
            &signingkey_obj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_CertifyCreation_Prepare(sys_context,
        signingkey_obj->handle, certifiedkey_obj->handle, policy_qualifier,
        creation_hash, in_scheme, creation_ticket);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_CertifyCreation_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, signingkey_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_certifycreation_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(esys_context, certifiedkey_obj->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_certifycreation_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(signingkey_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(signingkey_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_certifycreation_free_name1_name2:
        Esys_Free(name2);
tpm2_certifycreation_free_name1:
        Esys_Free(name1);
        goto tpm2_certifycreation_skip_esapi_call;
    }

    TSS2_RC rval = Esys_CertifyCreation(esys_context, signingkey_obj->tr_handle,
        certifiedkey_obj->tr_handle, signingkey_obj_session_handle,
        ESYS_TR_NONE, ESYS_TR_NONE, policy_qualifier, creation_hash, in_scheme,
        creation_ticket, certify_info, signature);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_CertifyCreation, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_certifycreation_skip_esapi_call:
    return rc;
}

tool_rc tpm2_setprimarypolicy(ESYS_CONTEXT *esys_context,
    tpm2_loaded_object *hierarchy_object, TPM2B_DIGEST *auth_policy,
    TPMI_ALG_HASH hash_algorithm, TPM2B_DIGEST *cp_hash) {

    ESYS_TR hierarchy_object_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context,
            hierarchy_object->tr_handle, hierarchy_object->session,
            &hierarchy_object_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_SetPrimaryPolicy_Prepare(sys_context,
            hierarchy_object->handle, auth_policy, hash_algorithm);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_SetPrimaryPolicy_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, hierarchy_object->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_setprimarypolicy_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(hierarchy_object->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(hierarchy_object->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_setprimarypolicy_free_name1:
        Esys_Free(name1);
        goto tpm2_setprimarypolicy_skip_esapi_call;
    }

    TSS2_RC rval = Esys_SetPrimaryPolicy(esys_context,
        hierarchy_object->tr_handle, hierarchy_object_session_handle,
        ESYS_TR_NONE, ESYS_TR_NONE, auth_policy, hash_algorithm);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_SetPrimaryPolicy, rval);
        return tool_rc_from_tpm(rval);
    }
tpm2_setprimarypolicy_skip_esapi_call:
    return rc;
}

tool_rc tpm2_quote(ESYS_CONTEXT *esys_context, tpm2_loaded_object *quote_obj,
        TPMT_SIG_SCHEME *in_scheme, TPM2B_DATA *qualifying_data,
        TPML_PCR_SELECTION *pcr_select, TPM2B_ATTEST **quoted,
        TPMT_SIGNATURE **signature, TPM2B_DIGEST *cp_hash) {

    ESYS_TR quote_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, quote_obj->tr_handle,
            quote_obj->session, &quote_obj_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Failed to get shandle");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Quote_Prepare(sys_context, quote_obj->handle,
        qualifying_data, in_scheme, pcr_select);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Quote_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, quote_obj->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_quote_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(quote_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(quote_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_quote_free_name1:
        Esys_Free(name1);
        goto tpm2_quote_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Quote(esys_context, quote_obj->tr_handle,
            quote_obj_session_handle, ESYS_TR_NONE, ESYS_TR_NONE,
            qualifying_data, in_scheme, pcr_select, quoted, signature);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_Quote, rval);
        return tool_rc_from_tpm(rval);
    }
tpm2_quote_skip_esapi_call:
    return rc;
}

tool_rc tpm2_changeeps(ESYS_CONTEXT *ectx,
    tpm2_session *platform_hierarchy_session) {

    ESYS_TR platform_hierarchy_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx, ESYS_TR_RH_PLATFORM,
        platform_hierarchy_session, &platform_hierarchy_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    TSS2_RC rval = Esys_ChangeEPS(ectx, ESYS_TR_RH_PLATFORM,
        platform_hierarchy_session_handle, ESYS_TR_NONE, ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ChangeEPS, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_changepps(ESYS_CONTEXT *ectx,
    tpm2_session *platform_hierarchy_session) {

    ESYS_TR platform_hierarchy_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx, ESYS_TR_RH_PLATFORM,
        platform_hierarchy_session, &platform_hierarchy_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

    TSS2_RC rval = Esys_ChangePPS(ectx, ESYS_TR_RH_PLATFORM,
        platform_hierarchy_session_handle, ESYS_TR_NONE, ESYS_TR_NONE);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ChangePPS, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_unseal(ESYS_CONTEXT *esys_context, tpm2_loaded_object *sealkey_obj,
        TPM2B_SENSITIVE_DATA **out_data, TPM2B_DIGEST *cp_hash) {

    ESYS_TR sealkey_obj_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(esys_context, sealkey_obj->tr_handle,
            sealkey_obj->session, &sealkey_obj_session_handle);
    if (rc != tool_rc_success) {
        return rc;
    }

        if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(esys_context, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_Unseal_Prepare(sys_context,
            sealkey_obj->handle);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_Unseal_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(esys_context, sealkey_obj->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_unseal_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(sealkey_obj->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(sealkey_obj->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_unseal_free_name1:
        Esys_Free(name1);
        goto tpm2_unseal_skip_esapi_call;
    }

    TSS2_RC rval = Esys_Unseal(esys_context, sealkey_obj->tr_handle,
            sealkey_obj_session_handle, ESYS_TR_NONE, ESYS_TR_NONE, out_data);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_Unseal, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_unseal_skip_esapi_call:
    return rc;
}

tool_rc tpm2_incrementalselftest(ESYS_CONTEXT *ectx, const TPML_ALG *to_test,
        TPML_ALG **to_do_list) {

    TSS2_RC rval = Esys_IncrementalSelfTest(ectx, ESYS_TR_NONE, ESYS_TR_NONE,
        ESYS_TR_NONE, to_test, to_do_list);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_IncrementalSelfTest, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_stirrandom(ESYS_CONTEXT *ectx,
        const TPM2B_SENSITIVE_DATA *data) {

    TSS2_RC rval = Esys_StirRandom(ectx, ESYS_TR_NONE, ESYS_TR_NONE,
        ESYS_TR_NONE, data);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_StirRandom, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_selftest(ESYS_CONTEXT *ectx, TPMI_YES_NO full_test) {

    TSS2_RC rval = Esys_SelfTest(ectx, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
        full_test);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_SelfTest, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_gettestresult(ESYS_CONTEXT *ectx, TPM2B_MAX_BUFFER **out_data,
        TPM2_RC *test_result) {

    TSS2_RC rval = Esys_GetTestResult(ectx, ESYS_TR_NONE, ESYS_TR_NONE,
            ESYS_TR_NONE, out_data, test_result);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_GetTestResult, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_loadexternal(ESYS_CONTEXT *ectx, const TPM2B_SENSITIVE *private,
        const TPM2B_PUBLIC *public, TPMI_RH_HIERARCHY hierarchy,
        ESYS_TR *object_handle) {

    TSS2_RC rval = Esys_LoadExternal(ectx,
            ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
            private, public, hierarchy,
            object_handle);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_LoadExternal, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_pcr_event(ESYS_CONTEXT *ectx,
        ESYS_TR pcr, tpm2_session *session,
        const TPM2B_EVENT *event_data,
        TPML_DIGEST_VALUES **digests) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx, pcr, session,
            &shandle1);
    if (rc != tool_rc_success) {
        return rc;
    }

    TSS2_RC rval = Esys_PCR_Event(ectx, pcr, shandle1, ESYS_TR_NONE,
            ESYS_TR_NONE, event_data, digests);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PCR_Event, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_getrandom(ESYS_CONTEXT *ectx, UINT16 count,
        TPM2B_DIGEST **random) {

    TSS2_RC rval = Esys_GetRandom(ectx, ESYS_TR_NONE, ESYS_TR_NONE,
        ESYS_TR_NONE, count, random);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_GetRandom, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_startup(ESYS_CONTEXT *ectx, TPM2_SU startup_type) {

    TSS2_RC rval = Esys_Startup(ectx, startup_type);
    if (rval != TPM2_RC_SUCCESS && rval != TPM2_RC_INITIALIZE) {
        LOG_PERR(Esys_Startup, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_pcr_reset(ESYS_CONTEXT *ectx, ESYS_TR pcr_handle) {

    TSS2_RC rval = Esys_PCR_Reset(ectx, pcr_handle, ESYS_TR_PASSWORD,
            ESYS_TR_NONE, ESYS_TR_NONE);
    if (rval != TSS2_RC_SUCCESS) {
        LOG_PERR(Esys_PCR_Reset, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_makecredential(ESYS_CONTEXT *ectx, ESYS_TR handle,
        const TPM2B_DIGEST *credential, const TPM2B_NAME *object_name,
        TPM2B_ID_OBJECT **credential_blob, TPM2B_ENCRYPTED_SECRET **secret) {

    TSS2_RC rval = Esys_MakeCredential(ectx, handle, ESYS_TR_NONE, ESYS_TR_NONE,
            ESYS_TR_NONE, credential, object_name, credential_blob,
            secret);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_MakeCredential, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_verifysignature(ESYS_CONTEXT *ectx, ESYS_TR key_handle,
        const TPM2B_DIGEST *digest, const TPMT_SIGNATURE *signature,
        TPMT_TK_VERIFIED **validation) {

    TSS2_RC rval = Esys_VerifySignature(ectx,
            key_handle, ESYS_TR_NONE, ESYS_TR_NONE,
            ESYS_TR_NONE, digest, signature, validation);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_VerifySignature, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_readclock(ESYS_CONTEXT *ectx, TPMS_TIME_INFO **current_time) {

    TSS2_RC rval = Esys_ReadClock(ectx,
            ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
            current_time);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ReadClock, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_setclock(ESYS_CONTEXT *ectx, tpm2_loaded_object *object,
UINT64 new_time, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx,
            object->tr_handle, object->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for lockout hierarchy");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(ectx, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_ClockSet_Prepare(sys_context,
            object->handle, new_time);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_ClockSet_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(ectx, object->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_setclock_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(object->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(object->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_setclock_free_name1:
        Esys_Free(name1);
        goto tpm2_setclock_skip_esapi_call;
    }

    TSS2_RC rval = Esys_ClockSet(ectx,
            object->tr_handle,
            shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            new_time);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ClockSet, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_setclock_skip_esapi_call:
    return rc;
}

tool_rc tpm2_clockrateadjust(ESYS_CONTEXT *ectx, tpm2_loaded_object *object,
        TPM2_CLOCK_ADJUST rate_adjust, TPM2B_DIGEST *cp_hash) {

    ESYS_TR shandle1 = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx,
            object->tr_handle, object->session, &shandle1);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for lockout hierarchy");
        return rc;
    }

    if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(ectx, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_ClockRateAdjust_Prepare(sys_context,
            object->handle, rate_adjust);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_ClockRateAdjust_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(ectx, object->tr_handle,
            &name1);
        if (rc != tool_rc_success) {
            goto tpm2_clockrateadjust_free_name1;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(object->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, NULL, NULL,
            tpm2_session_get_authhash(object->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_clockrateadjust_free_name1:
        Esys_Free(name1);
        goto tpm2_clockrateadjust_skip_esapi_call;
    }

    TSS2_RC rval = Esys_ClockRateAdjust(ectx,
            object->tr_handle,
            shandle1, ESYS_TR_NONE, ESYS_TR_NONE,
            rate_adjust);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_ClockRateAdjust, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_clockrateadjust_skip_esapi_call:
    return rc;
}

tool_rc tpm2_shutdown(ESYS_CONTEXT *ectx, TPM2_SU shutdown_type) {

    TSS2_RC rval = Esys_Shutdown(ectx,
            ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
            shutdown_type);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_Shutdown, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_gettime(ESYS_CONTEXT *ectx,
        tpm2_loaded_object *privacy_admin,
        tpm2_loaded_object *signing_object,
        const TPM2B_DATA *qualifying_data,
        const TPMT_SIG_SCHEME *scheme,
        TPM2B_ATTEST **time_info,
        TPMT_SIGNATURE **signature,
        TPM2B_DIGEST *cp_hash) {

    ESYS_TR privacy_admin_session_handle = ESYS_TR_NONE;
    tool_rc rc = tpm2_auth_util_get_shandle(ectx,
            privacy_admin->tr_handle, privacy_admin->session, &privacy_admin_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for privacy admin");
        return rc;
    }

    ESYS_TR sign_session_handle = ESYS_TR_NONE;
    rc = tpm2_auth_util_get_shandle(ectx,
            signing_object->tr_handle, signing_object->session, &sign_session_handle);
    if (rc != tool_rc_success) {
        LOG_ERR("Couldn't get shandle for signing key");
        return rc;
    }

       if (cp_hash) {
        /*
         * Need sys_context to be able to calculate CpHash
         */
        TSS2_SYS_CONTEXT *sys_context = NULL;
        rc = tpm2_getsapicontext(ectx, &sys_context);
        if(rc != tool_rc_success) {
            LOG_ERR("Failed to acquire SAPI context.");
            return rc;
        }

        TSS2_RC rval = Tss2_Sys_GetTime_Prepare(sys_context,
        privacy_admin->handle, signing_object->handle, qualifying_data, scheme);
        if (rval != TPM2_RC_SUCCESS) {
            LOG_PERR(Tss2_Sys_GetTime_Prepare, rval);
            return tool_rc_general_error;
        }

        TPM2B_NAME *name1 = NULL;
        rc = tpm2_tr_get_name(ectx, privacy_admin->tr_handle, &name1);
        if (rc != tool_rc_success) {
            goto tpm2_gettime_free_name1;
        }

        TPM2B_NAME *name2 = NULL;
        rc = tpm2_tr_get_name(ectx, signing_object->tr_handle, &name2);
        if (rc != tool_rc_success) {
            goto tpm2_gettime_free_name1_name2;
        }

        cp_hash->size = tpm2_alg_util_get_hash_size(
            tpm2_session_get_authhash(signing_object->session));
        rc = tpm2_sapi_getcphash(sys_context, name1, name2, NULL,
            tpm2_session_get_authhash(signing_object->session), cp_hash);

        /*
         * Exit here without making the ESYS call since we just need the cpHash
         */
tpm2_gettime_free_name1_name2:
        Esys_Free(name2);
tpm2_gettime_free_name1:
        Esys_Free(name1);
        goto tpm2_gettime_skip_esapi_call;
    }

    TSS2_RC rval = Esys_GetTime(
            ectx,
            privacy_admin->tr_handle,
            signing_object->tr_handle,
            privacy_admin_session_handle,
            sign_session_handle,
            ESYS_TR_NONE,
            qualifying_data,
            scheme,
            time_info,
            signature);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_GetTime, rval);
        return tool_rc_from_tpm(rval);
    }

tpm2_gettime_skip_esapi_call:
    return rc;
}

tool_rc tpm2_getsapicontext(ESYS_CONTEXT *esys_context,
    TSS2_SYS_CONTEXT **sys_context) {

    TSS2_RC rval = Esys_GetSysContext(esys_context, sys_context);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Esys_GetSysContext, rval);
        return tool_rc_from_tpm(rval);
    }

    return tool_rc_success;
}

tool_rc tpm2_sapi_getcphash(TSS2_SYS_CONTEXT *sys_context,
    const TPM2B_NAME *name1, const TPM2B_NAME *name2, const TPM2B_NAME *name3,
    TPMI_ALG_HASH halg, TPM2B_DIGEST *cp_hash) {

    uint8_t command_code[4];
    TSS2_RC rval = Tss2_Sys_GetCommandCode(sys_context, &command_code[0]);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Tss2_Sys_GetCommandCode, rval);
        return tool_rc_general_error;
    }

    const uint8_t *command_parameters;
    size_t command_parameters_size;
    rval = Tss2_Sys_GetCpBuffer(sys_context, &command_parameters_size,
        &command_parameters);
    if (rval != TPM2_RC_SUCCESS) {
        LOG_PERR(Tss2_Sys_GetCpBuffer, rval);
        return tool_rc_general_error;
    }

    uint16_t to_hash_len = sizeof(command_code) + command_parameters_size;
    to_hash_len += name1 ? name1->size : 0;
    to_hash_len += name2 ? name2->size : 0;
    to_hash_len += name3 ? name3->size : 0;

    uint8_t *to_hash = malloc(to_hash_len);
    if (!to_hash) {
        LOG_ERR("oom");
        return tool_rc_general_error;
    }

    //Command-Code
    memcpy(to_hash, command_code, sizeof(command_code));
    uint16_t offset = sizeof(command_code);

    //Names
    if (name1) {
        memcpy(to_hash + offset, name1->name, name1->size);
        offset += name1->size;
    }
    if (name2) {
        memcpy(to_hash + offset, name2->name, name2->size);
        offset += name2->size;
    }
    if (name3) {
        memcpy(to_hash + offset, name3->name, name3->size);
        offset += name3->size;
    }

    //CpBuffer
    memcpy(to_hash + offset, command_parameters, command_parameters_size);

    //cpHash
    tool_rc rc = tool_rc_success;
    bool result = tpm2_openssl_hash_compute_data(halg, to_hash, to_hash_len,
        cp_hash);
    free(to_hash);
    if (!result) {
        LOG_ERR("Failed cpHash digest calculation.");
        rc = tool_rc_general_error;
    }

    return rc;
}
