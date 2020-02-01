# SPDX-License-Identifier: BSD-3-Clause

source helpers.sh

alg_primary_obj=sha256
alg_primary_key=rsa
alg_create_obj=sha256
alg_create_key=hmac

file_primary_key_ctx=context.p_"$alg_primary_obj"_"$alg_primary_key"
file_loadexternal_key_pub=opu_"$alg_create_obj"_"$alg_create_key"
file_loadexternal_key_priv=opr_"$alg_create_obj"_"$alg_create_key"
file_loadexternal_key_name=name.loadexternal_"$alg_primary_obj"_\
"$alg_primary_key"-"$alg_create_obj"_"$alg_create_key"
file_loadexternal_key_ctx=ctx_loadexternal_out_"$alg_primary_obj"_\
"$alg_primary_key"-"$alg_create_obj"_"$alg_create_key"
file_loadexternal_output=loadexternal_"$file_loadexternal_key_ctx"

Handle_parent=0x81010019

cleanup() {
  rm -f $file_primary_key_ctx $file_loadexternal_key_pub \
  $file_loadexternal_key_priv $file_loadexternal_key_name \
  $file_loadexternal_key_ctx $file_loadexternal_output private.pem public.pem \
  plain.txt plain.rsa.dec key.ctx public.ecc.pem private.ecc.pem \
  data.in.digest data.out.signed ticket.out name.bin stdout.yaml passfile \
  private.pem

  if [ $(ina "$@" "keep_handle") -ne 0 ]; then
    tpm2_evictcontrol -Q -Co -c $Handle_parent 2>/dev/null || true
  fi

  if [ $(ina "$@" "no-shut-down") -ne 0 ]; then
    shut_down
  fi
}
trap cleanup EXIT

start_up

cleanup "no-shut-down"

tpm2_clear

run_tss_test() {

    tpm2_createprimary -Q -C e -g $alg_primary_obj -G $alg_primary_key \
    -c $file_primary_key_ctx

    tpm2_create -Q -g $alg_create_obj -G $alg_create_key \
    -u $file_loadexternal_key_pub -r $file_loadexternal_key_priv \
    -C $file_primary_key_ctx

    tpm2_loadexternal -Q -C n -u $file_loadexternal_key_pub \
    -c $file_loadexternal_key_ctx

    tpm2_evictcontrol -Q -C o -c $file_primary_key_ctx $Handle_parent

    # Test with Handle
    cleanup "keep_handle" "no-shut-down"

    tpm2_create -Q -C $Handle_parent -g $alg_create_obj -G $alg_create_key \
    -u $file_loadexternal_key_pub  -r  $file_loadexternal_key_priv

    tpm2_loadexternal -Q -C n -u $file_loadexternal_key_pub \
    -c $file_loadexternal_key_ctx

    # Test with default hierarchy (and handle)
    cleanup "keep_handle" "no-shut-down"

    tpm2_create -Q -C $Handle_parent -g $alg_create_obj -G $alg_create_key \
    -u $file_loadexternal_key_pub -r $file_loadexternal_key_priv

    tpm2_loadexternal -Q -u $file_loadexternal_key_pub \
    -c $file_loadexternal_key_ctx

    cleanup "no-shut-down"
}

# Test loading an OSSL generated private key with a password
run_rsa_test() {

    openssl genrsa -out private.pem $1
    openssl rsa -in private.pem -out public.pem -outform PEM -pubout

    echo "hello world" > plain.txt
    openssl rsautl -encrypt -inkey public.pem -pubin -in plain.txt \
    -out plain.rsa.enc

    tpm2_loadexternal -G rsa -C n -p foo -r private.pem -c key.ctx

    tpm2_rsadecrypt -c key.ctx -p foo -o plain.rsa.dec plain.rsa.enc

    diff plain.txt plain.rsa.dec

    # try encrypting with the public key and decrypting with the private
    tpm2_loadexternal -G rsa -C n -p foo -u public.pem -c key.ctx

    tpm2_rsaencrypt -c key.ctx plain.txt -o plain.rsa.enc

    openssl rsautl -decrypt -inkey private.pem -in plain.rsa.enc \
    -out plain.rsa.dec

    diff plain.txt plain.rsa.dec

    cleanup "no-shut-down"
}

#
# Verify loading an external AES key.
#
# Paramter 1: The AES keysize to create in bytes.
#
# Notes: Also tests that name output and YAML output are valid.
#
run_aes_test() {

    dd if=/dev/urandom of=sym.key bs=1 count=$(($1 / 8)) 2>/dev/null

    tpm2_loadexternal -G aes -r sym.key -n name.bin -c key.ctx > stdout.yaml

    local name1=$(yaml_get_kv "stdout.yaml" "name")
    local name2="$(xxd -c 256 -p name.bin)"

    test "$name1" == "$name2"

    echo "plaintext" > "plain.txt"

    tpm2_encryptdecrypt -c key.ctx -o plain.enc plain.txt

    openssl enc -in plain.enc -out plain.dec.ssl -d -K `xxd -c 256 -p sym.key` \
	-iv 0 -aes-$1-cfb

    diff plain.txt plain.dec.ssl

    cleanup "no-shut-down"
}

run_ecc_test() {
    #
    # Test loading an OSSL PEM format ECC key, and verifying a signature
    # external to the TPM
    #

    #
    # Generate a NIST P256 Private and Public ECC pem file
    #
    openssl ecparam -name $1 -genkey -noout -out private.ecc.pem
    openssl ec -in private.ecc.pem -out public.ecc.pem -pubout

    # Generate a hash to sign
    echo "data to sign" > data.in.raw
    shasum -a 256 data.in.raw | awk '{ print "000000 " $1 }' | xxd -r -c 32 > \
    data.in.digest

    # Load the private key for signing
    tpm2_loadexternal -Q -G ecc -r private.ecc.pem -c key.ctx

    # Sign in the TPM and verify with OSSL
    tpm2_sign -Q -c key.ctx -g sha256 -d -f plain -o data.out.signed \
    data.in.digest
    openssl dgst -verify public.ecc.pem -keyform pem -sha256 -signature \
    data.out.signed data.in.raw

    # Sign with openssl and verify with TPM but only with the public portion of
    # an object loaded
    tpm2_loadexternal -Q -G ecc -u public.ecc.pem -c key.ctx
    openssl dgst -sha256 -sign private.ecc.pem -out data.out.signed data.in.raw
    tpm2_verifysignature -Q -c key.ctx -g sha256 -m data.in.raw -f ecdsa \
    -s data.out.signed

    cleanup "no-shut-down"
}

run_rsa_passin_test() {

    openssl genrsa -aes128 -passout "pass:mypassword" -out "private.pem" 1024

    if [ "$2" != "stdin" ]; then
        cmd="tpm2_loadexternal -Q -G rsa -r $1 -c key.ctx --passin $2"
    else
        cmd="tpm2_loadexternal -Q -G rsa -r $1 -c key.ctx --passin $2 < $3"
    fi;

    eval $cmd

    cleanup "no-shut-down"
}

run_tss_test

run_rsa_test 1024
run_rsa_test 2048

run_aes_test 128
run_aes_test 256

run_ecc_test prime256v1

#
# Test loadexternal passin option
#
run_rsa_passin_test "private.pem" "pass:mypassword"

export envvar="mypassword"
run_rsa_passin_test "private.pem" "env:envvar"

echo -n "mypassword" > "passfile"
run_rsa_passin_test "private.pem" "file:passfile"

echo -n "mypassword" > "passfile"
exec 42<> passfile
run_rsa_passin_test "private.pem" "fd:42"

echo -n "mypassword" > "passfile"
run_rsa_passin_test "private.pem" "stdin" "passfile"


exit 0
