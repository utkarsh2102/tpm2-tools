#include <inttypes.h>
#include <stdlib.h>
#include <tss2/tss2_tpm2_types.h>

#include "log.h"
#include "efi_event.h"
#include "tpm2_alg_util.h"
#include "tpm2_eventlog.h"

bool digest2_accumulator_callback(TCG_DIGEST2 const *digest, size_t size,
                                  void *data){

    if (digest == NULL || data == NULL) {
        LOG_ERR("neither parameter may be NULL");
        return false;
    }
    size_t *accumulator = (size_t*)data;

    *accumulator += sizeof(*digest) + size;

    return true;
}
/*
 * Invoke callback function for each TCG_DIGEST2 structure in the provided
 * TCG_EVENT_HEADER2. The callback function is only invoked if this function
 * is first able to determine that the provided buffer is large enough to
 * hold the digest. The size of the digest is passed to the callback in the
 * 'size' parameter.
 */
bool foreach_digest2(TCG_DIGEST2 const *digest, size_t count, size_t size,
                     DIGEST2_CALLBACK callback, void *data) {

    if (digest == NULL) {
        LOG_ERR("digest cannot be NULL");
        return false;
    }

    bool ret = true;

    size_t i;
    for (i = 0; i < count; ++i) {
        if (size < sizeof(*digest)) {
            LOG_ERR("insufficient size for digest header");
            return false;
        }
        size_t alg_size = tpm2_alg_util_get_hash_size(digest->AlgorithmId);
        if (size < sizeof(*digest) + alg_size) {
            LOG_ERR("insufficient size for digest buffer");
            return false;
        }
        if (callback != NULL) {
            ret = callback(digest, alg_size, data);
            if (!ret) {
                LOG_ERR("callback failed for digest at %p with size %zu", digest, alg_size);
                break;
            }
        }
        size -= sizeof(*digest) + alg_size;
        digest = (TCG_DIGEST2*)((uintptr_t)digest->Digest + alg_size);
    }

    return ret;
}

/*
 * given the provided event type, parse event to ensure the structure / data
 * in the buffer doesn't exceed the buffer size
 */
bool parse_event2body(TCG_EVENT2 const *event, UINT32 type) {

    switch (type) {
    /* TCG PC Client FPF section 9.2.6 */
    case EV_EFI_VARIABLE_DRIVER_CONFIG:
    case EV_EFI_VARIABLE_BOOT:
    case EV_EFI_VARIABLE_AUTHORITY:
        {
            UEFI_VARIABLE_DATA *data = (UEFI_VARIABLE_DATA*)event->Event;
            if (event->EventSize < sizeof(*data)) {
                LOG_ERR("size is insufficient for UEFI variable data");
                return false;
            }

            if (event->EventSize < sizeof(*data) + data->UnicodeNameLength *
                sizeof(char16_t) + data->VariableDataLength)
            {
                LOG_ERR("size is insufficient for UEFI variable data");
                return false;
            }
        }
        break;
    /* TCG PC Client FPF section 9.2.5 */
    case EV_POST_CODE:
    case EV_S_CRTM_CONTENTS:
    case EV_EFI_PLATFORM_FIRMWARE_BLOB:
        {
            UEFI_PLATFORM_FIRMWARE_BLOB *data =
                (UEFI_PLATFORM_FIRMWARE_BLOB*)event->Event;
            UNUSED(data);
            if (event->EventSize < sizeof(*data)) {
                LOG_ERR("size is insufficient for UEFI FW blob data");
                return false;
            }
        }
        break;
    case EV_EFI_BOOT_SERVICES_APPLICATION:
    case EV_EFI_BOOT_SERVICES_DRIVER:
    case EV_EFI_RUNTIME_SERVICES_DRIVER:
        {
            UEFI_IMAGE_LOAD_EVENT *data = (UEFI_IMAGE_LOAD_EVENT*)event->Event;
            UNUSED(data);
            if (event->EventSize < sizeof(*data)) {
                LOG_ERR("size is insufficient for UEFI image load event");
                return false;
            }
            /* what about the device path? */
        }
        break;
    }

    return true;
}
/*
 * parse event structure, including header, digests and event buffer ensuring
 * it all fits within the provided buffer (buf_size).
 */
bool parse_event2(TCG_EVENT_HEADER2 const *eventhdr, size_t buf_size,
                  size_t *event_size, size_t *digests_size) {

    bool ret;

    if (buf_size < sizeof(*eventhdr)) {
        LOG_ERR("corrupted log, insufficient size for event header: %zu", buf_size);
        return false;
    }
    *event_size = sizeof(*eventhdr);

    ret = foreach_digest2(eventhdr->Digests, eventhdr->DigestCount,
                          buf_size - sizeof(*eventhdr),
                          digest2_accumulator_callback, digests_size);
    if (ret != true) {
        return false;
    }
    *event_size += *digests_size;

    TCG_EVENT2 *event = (TCG_EVENT2*)((uintptr_t)eventhdr + *event_size);
    if (buf_size < *event_size + sizeof(*event)) {
        LOG_ERR("corrupted log: size insufficient for EventSize");
        return false;
    }
    *event_size += sizeof(*event);

    if (buf_size < *event_size + event->EventSize) {
        LOG_ERR("size insufficient for event data");
        return false;
    }
    *event_size += event->EventSize;

    return true;
}

bool foreach_event2(TCG_EVENT_HEADER2 const *eventhdr_start, size_t size,
                    EVENT2_CALLBACK event2hdr_cb,
                    DIGEST2_CALLBACK digest2_cb,
                    EVENT2DATA_CALLBACK event2_cb, void *data) {

    if (eventhdr_start == NULL) {
        LOG_ERR("invalid parameter");
        return false;
    }
    if (size == 0) {
        return true;
    }

    TCG_EVENT_HEADER2 const *eventhdr;
    size_t event_size;
    bool ret;

    for (eventhdr = eventhdr_start, event_size = 0;
         size > 0;
         eventhdr = (TCG_EVENT_HEADER2*)((uintptr_t)eventhdr + event_size),
         size -= event_size) {

        size_t digests_size = 0;

        ret = parse_event2(eventhdr, size, &event_size, &digests_size);
        if (!ret) {
            return ret;
        }

        TCG_EVENT2 *event = (TCG_EVENT2*)((uintptr_t)eventhdr->Digests + digests_size);
        /* event header callback */
        if (event2hdr_cb != NULL) {
            ret = event2hdr_cb(eventhdr, event_size, data);
            if (ret != true) {
                return false;
            }
        }

        /* digest callback foreach digest */
        if (digest2_cb != NULL) {
            ret = foreach_digest2(eventhdr->Digests, eventhdr->DigestCount,
                                  digests_size, digest2_cb, data);
            if (ret != true) {
                return false;
            }
        }

        ret = parse_event2body(event, eventhdr->EventType);
        if (ret != true) {
            return ret;
        }

        /* event data callback */
        if (event2_cb != NULL) {
            ret = event2_cb(event, eventhdr->EventType, data);
            if (ret != true) {
                return false;
            }
        }
    }

    return true;
}

bool specid_event(TCG_EVENT const *event, size_t size,
                  TCG_EVENT_HEADER2 **next) {

    /* enough size for the 1.2 event structure */
    if (size < sizeof(*event)) {
        LOG_ERR("insufficient size for SpecID event header");
        return false;
    }

    if (event->eventType != EV_NO_ACTION) {
        LOG_ERR("SpecID eventType must be EV_NO_ACTION");
        return false;
    }

    if (event->pcrIndex != 0) {
        LOG_ERR("bad pcrIndex for EV_NO_ACTION event");
        return false;
    }

    size_t i;
    for (i = 0; i < sizeof(event->digest); ++i) {
        if (event->digest[i] != 0) {
            LOG_ERR("SpecID digest data malformed");
            return false;
        }
    }

    /* eventDataSize must be sufficient to hold the specid event */
    if (event->eventDataSize < sizeof(TCG_SPECID_EVENT)) {
        LOG_ERR("invalid eventDataSize in specid event");
        return false;
    }

    /* buffer size must be sufficient to hold event and event data */
    if (size < sizeof(*event) + (sizeof(event->event[0]) *
                                 event->eventDataSize)) {
        LOG_ERR("insufficient size for SpecID event data");
        return false;
    }

    /* specid event must have 1 or more algorithms */
    TCG_SPECID_EVENT *event_specid = (TCG_SPECID_EVENT*)event->event;
    if (event_specid->numberOfAlgorithms == 0) {
        LOG_ERR("numberOfAlgorithms is invalid, may not be 0");
        return false;
    }

    /* buffer size must be sufficient to hold event, specid event & algs */
    if (size < sizeof(*event) + sizeof(*event_specid) +
               sizeof(event_specid->digestSizes[0]) *
               event_specid->numberOfAlgorithms) {
        LOG_ERR("insufficient size for SpecID algorithms");
        return false;
    }

    /* size must be sufficient for event, specid, algs & vendor stuff */
    if (size < sizeof(*event) + sizeof(*event_specid) +
               sizeof(event_specid->digestSizes[0]) *
               event_specid->numberOfAlgorithms + sizeof(TCG_VENDOR_INFO)) {
        LOG_ERR("insufficient size for VendorStuff");
        return false;
    }

    TCG_VENDOR_INFO *vendor = (TCG_VENDOR_INFO*)((uintptr_t)event_specid->digestSizes +
                                                 sizeof(*event_specid->digestSizes) *
                                                 event_specid->numberOfAlgorithms);
    /* size must be sufficient for vendorInfo */
    if (size < sizeof(*event) + sizeof(*event_specid) +
               sizeof(event_specid->digestSizes[0]) *
               event_specid->numberOfAlgorithms + sizeof(*vendor) +
               vendor->vendorInfoSize) {
        LOG_ERR("insufficient size for VendorStuff data");
        return false;
    }
    *next = (TCG_EVENT_HEADER2*)((uintptr_t)vendor->vendorInfo + vendor->vendorInfoSize);

    return true;
}

bool parse_eventlog(BYTE const *eventlog, size_t size,
                    SPECID_CALLBACK specid_cb,
                    EVENT2_CALLBACK event2hdr_cb,
                    DIGEST2_CALLBACK digest2_cb,
                    EVENT2DATA_CALLBACK event2_cb, void *data)
{

    TCG_EVENT_HEADER2 *next;
    TCG_EVENT *event = (TCG_EVENT*)eventlog;
    bool ret;

    ret = specid_event(event, size, &next);
    if (!ret) {
        return false;
    }

    size -= (uintptr_t)next - (uintptr_t)eventlog;

    if (specid_cb) {
        ret = specid_cb(event, data);
        if (!ret) {
            return false;
        }
    }

    return foreach_event2(next, size, event2hdr_cb, digest2_cb, event2_cb, data);
}
