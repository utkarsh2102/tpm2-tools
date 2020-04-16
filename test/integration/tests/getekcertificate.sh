# SPDX-License-Identifier: BSD-3-Clause

source helpers.sh

cleanup() {
    rm -f test_rsa_ek.pub rsa_ek_cert.bin stdout_rsa_ek_cert.bin \
          test_ecc_ek.pub ecc_ek_cert.bin stdout_ecc_ek_cert.bin

    shut_down
}
trap cleanup EXIT

start_up

# Check connectivity
if [ -z "$(curl -V 2>/dev/null)" ]; then
    echo "curl is not not installed. Skipping connection check."
else
    if [ "$(curl --silent --output /dev/null --write-out %{http_code} \
    'https://ekop.intel.com/')" != '200' ]; then
        echo 'No connection to https://ekop.intel.com/'
        exit 77
    fi
fi

# Sample RSA ek public from a real platform
echo "013a0001000b000300b20020837197674484b3f81a90cc8d46a5d724fd52
d76e06520b64f2a1da1b331469aa00060080004300100800000000000100
c320e2f244a8601aacf3e01d26c665249935562de1da197e9e7f076c4696
13cfb653e98ec2c386fc1d133f2c8c6cc338b732f0b208bd838a877a3e5b
bc3e1d4084e835c7c8906a1c05b4d2d30fdbebc1dbad950fa6b165bd4b6a
864603146164c0c4f59d489011ef1f928deea6e90061f3d375e564627315
1ef622252098be1a4ab01dc0a12227c609fdaceb115af408d4693a6f4991
9774695b0c12bc18a1ff7120a7337b2fb5f1951d8bb7f094d5b554c11c95
23b30729fe64787d0a13b9e630488dab4dfd86634a5270ec72fcc5a44dc6
79a8f32938dd8197e29dae839f5b4ca0f5de27c9522c23c54e1c2ce57859
525118bd4470b18180eef78ae4267bcd" | xxd -r -p > test_rsa_ek.pub

# Get ek certificate and output to file
tpm2_getekcertificate -u test_rsa_ek.pub -x -X -o rsa_ek_cert.bin

# Test that stdoutput is the same
tpm2_getekcertificate -u test_rsa_ek.pub -x -X > stdout_rsa_ek_cert.bin

# stdout file should match
cmp rsa_ek_cert.bin stdout_rsa_ek_cert.bin

# Retrieved certificate should be valid
if [ "$(md5sum rsa_ek_cert.bin| awk '{ print $1 }')" != \
"56af9eb8a271bbf7ac41b780acd91ff5" ]; then
 echo "Failed: retrieving RSA endorsement key certificate"
 exit 1
fi

# Sample ECC ek public from a real platform
echo "007a0023000b000300b20020837197674484b3f81a90cc8d46a5d724fd52
d76e06520b64f2a1da1b331469aa00060080004300100003001000206d8e
7630ee5d11e566e80299bfb9e43cec8c44f70bc8ad81b50f690a3deb7498
002021a536c8fef7482313d7f4517f11c9f2b4cd424cbc8fe9094b895668
51fe0853" | xxd -r -p > test_ecc_ek.pub

# Get ecc certificate and output to file
tpm2_getekcertificate -u test_ecc_ek.pub -x -X -o ecc_ek_cert.bin

# Test that stdoutput is the same
tpm2_getekcertificate -u test_ecc_ek.pub -x -X > stdout_ecc_ek_cert.bin

# stdout file should match
cmp ecc_ek_cert.bin stdout_ecc_ek_cert.bin

# Retrieved certificate should be valid
if [ "$(md5sum ecc_ek_cert.bin| awk '{ print $1 }')" != \
"48a7fdb9ad2eec6a71f67092c54770f9" ]; then
 echo "Failed: retrieving ecc endorsement key certificate"
 exit 1
fi

exit 0
