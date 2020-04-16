# SPDX-License-Identifier: BSD-3-Clause

set -E

function filter_algs_by() {

python << pyscript
from __future__ import print_function

import sys
import yaml

with open("$1") as f:
    try:
        y = yaml.safe_load(f)
        for alg, details in y.items():
            if $2:
                print(alg)
    except yaml.YAMLError as exc:
        sys.exit(exc)
pyscript
}

populate_algs() {
    algs="$(mktemp)"
    tpm2_getcap algorithms > "${algs}"
    filter_algs_by "${algs}" "${1}"
    rm "${algs}"
}

populate_hash_algs() {
    populate_algs "details['hash'] and not details['method'] and not details['symmetric'] and not details['signing'] $1"
}

# Return alg argument if supported by TPM.
hash_alg_supported() {
    local orig_alg="$1"
    local alg="$orig_alg"
    local algs_supported

    algs_supported="$(populate_hash_algs name)"
    local hex2name=(
        [0x04]="sha1"
        [0x0B]="sha256"
        [0x0C]="sha384"
        [0x0D]="sha512"
        [0x12]="sm3_256"
    )

    if [ -z "$alg" ]; then
        echo "$algs_supported"
        return
    fi

    if [ "$alg" = "${alg//[^0-9a-fA-FxX]/}" ]; then
        alg=${hex2name["$alg"]}
        [ -z "$alg" ] && return
    fi

    local t_alg
    for t_alg in $algs_supported; do
        if [ "$t_alg" = "$alg" ]; then
            echo "$orig_alg"
            return
        fi
    done
}

#
# Verifies that the contexts of a file path provided
# as the first argument loads as a YAML file.
#
function yaml_verify() {
python << pyscript
from __future__ import print_function

import sys
import yaml

with open("$1") as f:
    try:
        y = yaml.safe_load(f)
    except yaml.YAMLError as exc:
        sys.exit(exc)
pyscript
}

#
# Given a file as argument 1, prints the value of the key
# provided as argument 2 and optionally argument 3 (for nested maps).
# Note that all names and values are parsed as strings.
#
function yaml_get_kv() {

    third_arg=""
    if [ $# -eq 3 ]; then
        third_arg=$3
    fi

python << pyscript
from __future__ import print_function

import sys
import yaml

with open("$1") as f:
    try:
        y = yaml.load(f, Loader=yaml.BaseLoader)
        if $# == 3:
            print(y["$2"]["$third_arg"])
        else:
            print(y["$2"])
    except yaml.YAMLError as exc:
        sys.exit(exc)
pyscript
}

function recreate_info() {
    echo
    echo "--- To recreate this test run the following: ---"
    local a="export TPM2_ABRMD=\"$TPM2_ABRMD\"\n"
    a="$a""export TPM2_SIM=\"$TPM2_SIM\"\n"
    a="$a""export TPM2ABRMD_TCTI=\"$TPM2ABRMD_TCTI\"\n"
    a="$a""export TPM2_SIMPORT=\"$TPM2_SIMPORT\"\n"
    a="$a""export TPM2TOOLS_TEST_TCTI=\"$TPM2TOOLS_TEST_TCTI\"\n"
    a="$a""export PATH=\"$PATH\"\n"
    a="$a""TPM2_SIM_NV_CHIP=\"$TPM2_SIM_NV_CHIP\"\n"
    a="$a""TPM2_TOOLS_TEST_FIXTURES=\"$TPM2_TOOLS_TEST_FIXTURES\"\n"
    echo "#!/usr/bin/env bash"
    echo -e "$a"
    local script="$tpm2_test_original_cwd""/""$0"
    echo $(realpath "$script")
    echo "--- EOF ---"
    echo
}

tpm2_test_original_cwd=""
tpm2_test_cwd=""
function switch_to_test_dir() {
    tpm2_test_original_cwd=`pwd`;
    tpm2_test_cwd=$(mktemp -d ${TMPDIR:-/tmp}/tpm2_test_XXXXXX)
    echo "creating simulator working dir: $tpm2_test_cwd"
    pushd "$tpm2_test_cwd"
    echo "Switched to CWD: $(pwd)"
}

function switch_back_from_test_dir() {
    popd
}

tpm2_sim_pid=""
tpm2_abrmd_pid=""
tpm2_tcti_opts=""
tpm2tools_tcti=""

sock_tool="unknown"
OS=$(uname)

if [ "$OS" == "Linux" ]; then
    sock_tool="ss -lntp4"
elif [ "$OS" == "FreeBSD" ]; then
    sock_tool="sockstat -l4"
fi

function start_sim() {
    local max_cnt=10

    # if a user is specifying the sim port, then only attempt it once
    if [ -n "$TPM2_SIMPORT" ]; then
        max_cnt=1
    fi

    while [ $max_cnt -gt 0 ]; do
        # If either the requested simulator port or the port that will be used
        # by mssim TCTI which is tpm2_sim_port + 1 is occupied (ESTABLISHED, TIME_WAIT, etc...),
        # just continue up to 10 retries
        # (See : https://github.com/tpm2-software/tpm2-tss/blob/master/src/tss2-tcti/tcti-mssim.c:559)
        if [ -z "$TPM2_SIMPORT" ]; then
            tpm2_sim_port="$(shuf -i 2321-65534 -n 1)"
        else
            tpm2_sim_port=$TPM2_SIMPORT
        fi
        tpm2_sim_cmd_port=$((tpm2_sim_port + 1))
        echo "Attempting to start simulator on port: $tpm2_sim_port"
        $TPM2_SIM -port $tpm2_sim_port &
        tpm2_sim_pid=$!
        sleep 1

        ${sock_tool} 2>/dev/null | grep ${TPM2_SIM} | grep ${tpm2_sim_pid} | grep ${tpm2_sim_port}
        tpm2_sim_port_rc=$?
        ${sock_tool} 2>/dev/null | grep ${TPM2_SIM} | grep ${tpm2_sim_pid} | grep ${tpm2_sim_cmd_port}
        tpm2_sim_cmd_port_rc=$?

        if [[ $tpm2_sim_port_rc -eq 0 ]] && [[ $tpm2_sim_cmd_port_rc -eq 0 ]]; then
            echo "Started simulator on port $tpm2_sim_port in dir \"$PWD\""
            TPM2_SIMPORT=$tpm2_sim_port
            # set a possible tools tcti to use mssim
            tpm2tools_tcti="mssim:host=localhost,port=$TPM2_SIMPORT"
            echo "tpm2tools_tcti=\"$tpm2tools_tcti\""
            return 0
        else
            echo "Could not start simulator at port: $tpm2_sim_port"
            kill "$tpm2_sim_pid"
            let "max_cnt=max_cnt-1"
            echo "Tries left: $max_cnt"
        fi
    done

    echo "Maximum attempts reached. Aborting"
    return 1
}

function start_abrmd() {

        local tpm2_tabrmd_opts

        # if we don't have an explicit TCTI to connect to, generate it
        if [ -z "$TPM2ABRMD_TCTI" ]; then
            echo "TPM2ABRMD_TCTI is empty, configuring"

            if [ -z "$TPM2_SIMPORT" ]; then
                echo "No simulator port found, can not determine ABRMD TCTI conf"
                return 1
            fi

            # TCTI information for use with ABRMD
            local name="com.intel.tss2.Tabrmd${TPM2_SIMPORT}"
            tpm2_tabrmd_opts="--session --dbus-name=$name --tcti=mssim:port=$TPM2_SIMPORT"
            echo "TPM2ABRMD_TCTI=\"$tpm2_tabrmd_opts\""
            TPM2ABRMD_TCTI="$tpm2_tabrmd_opts"
        fi

    if [ $UID -eq 0 ]; then
        tpm2_tabrmd_opts="--allow-root $tpm2_tabrmd_opts"
    fi

    echo "tpm2-abrmd command: $TPM2_ABRMD $tpm2_tabrmd_opts $TPM2ABRMD_TCTI"
    $TPM2_ABRMD $tpm2_tabrmd_opts $TPM2ABRMD_TCTI &
    tpm2_abrmd_pid=$!
    sleep 2

    if ! kill -0 "$tpm2_abrmd_pid"; then
        (>&2 echo "Could not start tpm2-abrmd \"$TPM2_ABRMD\", exit code: $?")
        kill -9 $tpm2_abrmd_pid
        return 1
    fi

    # set a possible tools tcti to use abrmd
    tpm2tools_tcti="tabrmd:bus_type=session,bus_name=$name"
    echo "tpm2tools_tcti=\"$tpm2tools_tcti\""

    return 0
}

#
# This start up routine performs the following actions and should
# be called by testing scripts if they need a TCTI. It also outputs
# information for how to recreate the test outside of the test harness.
#
# 1. Start the simulator if specified via env var TPM2_SIM. if TPM2_SIMPORT
#    is set, it attempts to start the simulator AT that port, else it tries
#    a random port, and sets TPM2_SIMPORT to the random port if successful.
#
# 2. Start abrmd if specified via env var TPM2_ABRMD. if TPM2ABRMD_TCTI is
#    set it starts abrmd using that TCTI, else it uses the value of TPM2_SIMPORT.
#
# 3. Pick a TCTI for the tools based on:
#    a) TPM2TOOLS_TEST_TCTI user specified, just use it.
#    b) TPM2TOOLS_TEST_TCTI not specified, the start_sim and start_anrmd routines
#       set tpm2tools_tcti variable, so use that.
#
function start_up() {

    switch_to_test_dir

    run_startup=true

    if [ -n "$TPM2_SIM" ]; then
        # Start the simulator
        echo "Starting the simulator"
        start_sim || exit 1
        echo "Started the simulator"
    else
        echo "not starting simulator"
    fi

    if [ -n "$TPM2_ABRMD" ]; then
        echo "Starting tpm2-abrmd"
        # Start tpm2-abrmd
        start_abrmd || exit 1
        run_startup=false
    else
        echo "not starting abrmd"
    fi

    echo "TPM2TOOLS_TEST_TCTI=$TPM2TOOLS_TEST_TCTI"
    if [ -z "$TPM2TOOLS_TEST_TCTI" ]; then
        echo "TPM2TOOLS_TEST_TCTI not set, attempting to figure out default"
        if [ -z "$tpm2tools_tcti" ]; then
            echo "The simulator not abrmd was started, cannot determine a TCTI for tools."
            exit 1;
        fi
        TPM2TOOLS_TEST_TCTI="$tpm2tools_tcti"
    fi

    echo "export TPM2TOOLS_TCTI=\"$TPM2TOOLS_TEST_TCTI\""
    export TPM2TOOLS_TCTI="$TPM2TOOLS_TEST_TCTI"

    recreate_info

    echo "run_startup: $run_startup"

    if [ $run_startup = true ]; then
        tpm2_startup -c
    fi

    if ! tpm2_clear; then
        exit 1
    fi
}

function shut_down() {

    echo "Shutting down"

    switch_back_from_test_dir

    fail=0
    if [ -n "$tpm2_abrmd_pid" ]; then
        if kill -0 "$tpm2_abrmd_pid"; then
            if ! kill -9 "$tpm2_abrmd_pid"; then
                (>&2 echo "ERROR: could not kill tpm2_abrmd on pid: $tpm2_abrmd_pid")
                fail=1
            fi
        else
            (>&2 echo "WARNING: tpm2_abrmd already stopped ($tpm2_abrmd_pid)")
        fi
    fi
    tpm2_abrmd_pid=""

    if [ -n "$tpm2_sim_pid" ]; then
        if kill -0 "$tpm2_sim_pid"; then
            if ! kill -9 "$tpm2_sim_pid"; then
                (>&2 echo "ERROR: could not kill tpm2 simulator on pid: $tpm2_sim_pid")
                fail=1
            fi
        else
            (>&2 echo "WARNING: TPM simulator already stopped ($tpm2_sim_pid)")
        fi
    fi
    tpm2_sim_pid=""

    echo "Removing sim dir: $tpm2_test_cwd"
    rm -rf "$tpm2_test_cwd" 2>/dev/null

    if [ $fail -ne 0 ]; then
        exit 1
    fi
}

#
# Set the default EXIT handler to always shut down, tests
# can override this.
#
trap shut_down EXIT

#
# Set the default on ERR handler to print the line number
# and failed command.
#
onerror() {
    echo "$BASH_COMMAND on line ${BASH_LINENO[0]} failed: $?"
    exit 1
}
trap onerror ERR

#
# print 0 if the list of arguments 1 to n-1 contains the last argument n
# print 1 otherwise
#
function ina() {
    local n=$#
    local value=${!n}
    for ((i=1;i < $#;i++)) {
        if [ "${!i}" == "${value}" ]; then
            echo 0
            return
        fi
    }
    echo 1
}

# Causes a test to skip by exiting with error code 77
# See the automake manual for the exit codes:
#   https://www.gnu.org/software/automake/manual/html_node/Scripts_002dbased-Testsuites.html
function skip_test() {
	exit 77
}

function setup_fapi() {
    tempdir=`pwd`/$(mktemp -d tss2_fapi.XXXXXX)

    KEYSTORE_USER=keystore_user
    KEYSTORE_SYSTEM=keystore_system
    LOG_DIR=log
    PROFILE_NAME=P_RSA

    mkdir -p $tempdir/$KEYSTORE_USER/policy $tempdir/$KEYSTORE_SYSTEM/policy \
        $tempdir/$LOG_DIR

cat > $tempdir/fapi_config.json <<EOF
{
    "profile_name": "${PROFILE_NAME}",
    "profile_dir": "$tempdir/",
    "user_dir": "$tempdir/${KEYSTORE_USER}",
    "system_dir": "$tempdir/${KEYSTORE_SYSTEM}",
    "tcti": "${TPM2TOOLS_TCTI}",
    "system_pcrs" : [],
    "ek_cert_less": "yes",
    "log_dir" : "$tempdir/${LOG_DIR}",
}
EOF

    export TSS2_FAPICONF=$tempdir/fapi_config.json
    export TEMP_DIR=$tempdir

    PATH=${BUILDDIR}/tools/fapi:$PATH

    setup_profile $tempdir
    setup_policies $tempdir
    resetPCR16

}

# Reset PCR 16. Important when using physical TPM
function resetPCR16(){
    tpm2_pcrreset 16
}

function setup_profile() {
# Setup Profile
cat > $tempdir/${PROFILE_NAME}.json <<EOF
{
    "type": "TPM2_ALG_RSA",
    "nameAlg":"TPM2_ALG_SHA256",
    "srk_template": "system,restricted,decrypt",
    "srk_persistent": 0,
    "ek_template":  "system,restricted,decrypt",
    "ecc_signing_scheme": {
        "scheme":"TPM2_ALG_ECDSA",
        "details":{
            "hashAlg":"TPM2_ALG_SHA256"
        },
    },
    "rsa_signing_scheme": {
        "scheme":"TPM2_ALG_RSAPSS",
        "details":{
            "hashAlg":"TPM2_ALG_SHA256"
        }
    },
    "rsa_decrypt_scheme": {
        "scheme":"TPM2_ALG_OAEP",
        "details":{
            "hashAlg":"TPM2_ALG_SHA256"
        }
    },
    "sym_mode":"TPM2_ALG_CFB",
    "sym_parameters": {
        "algorithm":"TPM2_ALG_AES",
        "keyBits":"128",
        "mode":"TPM2_ALG_CFB"
    },
    "sym_block_size": 16,
    "pcr_selection": [
        { "hash": "TPM2_ALG_SHA1",
          "pcrSelect": [ 9, 15, 13 ]
        },
        { "hash": "TPM2_ALG_SHA256",
          "pcrSelect": [ 8, 16, 14 ]
        }
    ],
    "exponent": 0,
    "keyBits": 2048
}
EOF
}

function setup_policies() {
    tempdir=$1

# Setup Policy Authorize
cat > $tempdir/pol_authorize.json <<EOF
{
    "description":"Description pol_authorize",
    "policy":[
        {
            "type": "POLICYAUTHORIZE",
            "keyPath": "/HS/SRK/myPolicySignKey"
        }
    ]
}
EOF

# Setup Policy Authorize with reference value
cat > $tempdir/pol_authorize_ref.json <<EOF
{
    "description":"Description pol_authorize",
    "policy":[
        {
            "type": "POLICYAUTHORIZE",
            "keyPath": "/HS/SRK/myPolicySignKey",
            "policyRef": "f0f1f2f3f4f5f6f7f8f9"
        }
    ]
}
EOF

# Setup Policy Authorize NV
cat > $tempdir/pol_authorize_nv.json <<EOF
{
    "description":"Description pol_authorize_nv",
    "policy":[
        {
            "type": "POLICYAUTHORIZENV",
            "nvPath": "/nv/Owner/myNV",
        }
  ]
}
EOF

# Setup Policy Duplicate
cat > $tempdir/pol_duplicate.json <<EOF
{
    "description":"Description pol_duplicate",
    "policy":[
        {
            "type": "POLICYDUPLICATIONSELECT",
            "newParentPath": "ext/myNewParent",
        }
    ]
}
EOF

# Setup Policy PCR
cat > $tempdir/pol_pcr16_0.json <<EOF
{
    "description":"Description pol_16_0",
    "policy":[
        {
            "type":"POLICYPCR",
            "pcrs":[
                {
                    "pcr":16,
                    "hashAlg":"TPM2_ALG_SHA",
                    "digest":"0000000000000000000000000000000000000000"
                }
            ]
        }
    ]
}
EOF

# Setup Policy Signed
cat > $tempdir/pol_signed.json <<EOF
{
    "description":"Description pol_signed",
    "policy":[
        {
            "type": "POLICYSIGNED",
            "keyPEM": "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAoGL6IrCSAznmIIzBessI\nmW7tPOUy78uWTIaub32KnYHn78KXprrZ3ykp6WDrOQeMjv4AA+14mJbg77apVYXy\nEnkFdOMa1hszSJnp6cJvx7ILngLvFUxzbVki\/ehvgS3nRk67Njal+nMTe8hpe3UK\nQeV\/Ij+F0r6Yz91W+4LPmncAiUesRZLetI2BZsKwHYRMznmpIYpoua1NtS8QpEXR\nMmsUue19eS\/XRAPmmCfnb5BX2Tn06iCpk6wO+RfMo9etcX5cLSAuIYEQYCvV2\/0X\nTfEw607vttBN0Y54LrVOKno1vRXd5sxyRlfB0WL42F4VG5TfcJo5u1Xq7k9m9K57\n8wIDAQAB\n-----END PUBLIC KEY-----\n"
        }
    ]
}
EOF

}
