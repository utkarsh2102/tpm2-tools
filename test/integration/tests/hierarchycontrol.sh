# SPDX-License-Identifier: BSD-3-Clause

source helpers.sh

cleanup() {

  if [ "$1" != "no-shut-down" ]; then
      shut_down
  fi
}
trap cleanup EXIT

start_up

cleanup "no-shut-down"

# Storage hierarchy
tpm2_hierarchycontrol -C p shEnable set
tpm2_hierarchycontrol -C p shEnable clear
tpm2_hierarchycontrol -C p shEnable set
tpm2_hierarchycontrol -C o shEnable clear

# Endorsement hierarchy
tpm2_hierarchycontrol -C p ehEnable set
tpm2_hierarchycontrol -C p ehEnable clear
tpm2_hierarchycontrol -C p ehEnable set
tpm2_hierarchycontrol -C e ehEnable clear

# Platform NV
tpm2_hierarchycontrol -C p phEnableNV set
tpm2_hierarchycontrol -C p phEnableNV clear
tpm2_hierarchycontrol -C p phEnableNV set

# Platform hierarchy
tpm2_hierarchycontrol -C p phEnable clear

# 0 the handler
trap - ERR

# ERROR: phEnable may not be 1 using this command
tpm2_hierarchycontrol -C p phEnable set

# EROOR: Only platform hierarchy handle can be specified for 1
tpm2_hierarchycontrol -C o shEnable set
tpm2_hierarchycontrol -C o ehEnable set
tpm2_hierarchycontrol -C o phEnable set
tpm2_hierarchycontrol -C o phEnableNV set
tpm2_hierarchycontrol -C e shEnable set
tpm2_hierarchycontrol -C e ehEnable set
tpm2_hierarchycontrol -C e phEnable set
tpm2_hierarchycontrol -C e phEnableNV set

# ERROR: Permanent handle lockout not supported by this command
tpm2_hierarchycontrol -C l shEnable set
tpm2_hierarchycontrol -C l ehEnable set
tpm2_hierarchycontrol -C l phEnable set
tpm2_hierarchycontrol -C l phEnableNV set
tpm2_hierarchycontrol -C l shEnable clear
tpm2_hierarchycontrol -C l ehEnable clear
tpm2_hierarchycontrol -C l phEnable clear
tpm2_hierarchycontrol -C l phEnableNV clear

# ERROR: Only platform and its authorization can be specified for 0
tpm2_hierarchycontrol -C o ehEnable clear
tpm2_hierarchycontrol -C o phEnable clear
tpm2_hierarchycontrol -C o phEnableNV clear
tpm2_hierarchycontrol -C e shEnable clear
tpm2_hierarchycontrol -C e phEnable clear
tpm2_hierarchycontrol -C e phEnableNV clear

exit 0
