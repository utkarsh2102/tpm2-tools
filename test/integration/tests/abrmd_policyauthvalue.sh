# SPDX-License-Identifier: BSD-3-Clause

source helpers.sh

policyauthvalue=policy.dat
session_ctx=session.ctx
o_policy_digest=policy.digest
primary_key_ctx=prim.ctx
key_ctx=key.ctx
key_pub=key.pub
key_priv=key.priv
plain_txt=plain.txt
signature_txt=signature.txt
testpswd=testpswd

cleanup() {
    rm -f $policyauthvalue $session_ctx $o_policy_digest $primary_key_ctx \
    $key_ctx $key_pub $key_priv $plain_txt $signature_txt

    tpm2 flushcontext $session_ctx 2>/dev/null || true

    if [ "${1}" != "no-shutdown" ]; then
        shut_down
    fi
}
trap cleanup EXIT

start_up

cleanup "no-shutdown"

echo "plaintext" > $plain_txt

tpm2 startauthsession -S $session_ctx
tpm2 policyauthvalue -S $session_ctx -L $policyauthvalue
tpm2 flushcontext $session_ctx
rm $session_ctx

tpm2 createprimary -C o -c $primary_key_ctx

tpm2 create -g sha256 -G ecc -u $key_pub -r $key_priv -C $primary_key_ctx \
-L $policyauthvalue -p $testpswd

tpm2 load -C $primary_key_ctx -u $key_pub -r $key_priv -c $key_ctx
tpm2 sign -c $key_ctx -p $testpswd -o $signature_txt $plain_txt
tpm2 verifysignature -c key.ctx -m $plain_txt -s $signature_txt

tpm2 startauthsession --policy-session -S $session_ctx
tpm2 policyauthvalue -S $session_ctx -L $policyauthvalue
tpm2 sign -c $key_ctx -p session:$session_ctx+$testpswd -o $signature_txt $plain_txt
tpm2 verifysignature -c key.ctx -m $plain_txt -s $signature_txt
tpm2 flushcontext $session_ctx
rm $session_ctx

exit 0
