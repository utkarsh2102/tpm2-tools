# bash completion for tmp2-tools                           -*- shell-script -*-

_tpm2_tools()
{
    local cur prev words cword split
    _init_completion -s || return
    local common_options=(-h --help -v --version -V --verbose -Q --quiet -Z --enable-errata -T --tcti=)
    local halg=(sha1 sha256 sha384 sha512 sm3_256)
    local public_object_alg=(rsa keyedhash ecc 0x25 symcipher)
    local signing_alg=(hmac rsassa rsapss ecdsa ecdaa sm2 ecschnorr)
    local signing_schemes=(hmac rsassa rsaes rsapss oeap)
    local tcti_opts=(device: mssim: abrmd:)

    case $prev in
      -g | --halg)
          if [[ "${COMP_WORDS[0]}" != "tpm2_createek" && "${COMP_WORDS[0]}" != "tpm2_getmanufec" && "${COMP_WORDS[0]}" != "tpm2_createak" ]]; then
            COMPREPLY=( $(compgen -W "${halg[*]}" -- "$cur") )
          else
            COMPREPLY=( $(compgen -W "${public_object_alg[*]}" -- "$cur") )
          fi
          return;;
      -G | --kalg)
          if [[  "${COMP_WORDS[0]}" != "tpm2_import"  && "${COMP_WORDS[0]}" != "tpm2_quote" ]]; then
            COMPREPLY=( $(compgen -W "${public_object_alg[*]}" -- "$cur") )
          else
            COMPREPLY=( $(compgen -W "${halg[*]}" -- "$cur") )
          fi
          return;;
      -D | --digest-alg)
          if [[  "${COMP_WORDS[0]}" == "tpm2_createak" ]]; then
            COMPREPLY=( $(compgen -W "${halg[*]}" -- "$cur") )
          else
            _filedir
          fi
          return;;

      -h | --help)
          COMPREPLY=( $(compgen -W 'man no-man' -- "$cur") )
          return;;
      -T | --tcti)
          COMPREPLY=( $(compgen -W "${tcti_opts[*]}" -- "$cur") )
          [[ $COMPREPLY == *: ]] && compopt -o nospace
          return;;
      -f)
          if [[ "${COMP_WORDS[0]}" == "tpm2_activatecredential" || "${COMP_WORDS[0]}" == "print" ]]; then
            _filedir
          elif [[ "${COMP_WORDS[0]}" == "tpm2_quote" || "${COMP_WORDS[0]}" == "tpm2_sign" ]]; then
            COMPREPLY=( $(compgen -W 'plain tss' -- "$cur") )
          elif [[ "${COMP_WORDS[0]}" == "tpm2_verifysignature" ]]; then
            COMPREPLY=( $(compgen -W "${signing_schemes[*]}" -- "$cur") )
          fi
          return;;
      -s)
          if [[ "${COMP_WORDS[0]}" == "tpm2_createak" ]]; then
            COMPREPLY=( $(compgen -W "${signing_alg[*]}" -- "$cur") )
          elif [["${COMP_WORDS[0]}" == "tpm2_verifysignature" ]];then
            _filedir
          fi
          return;;
      -u | -r)
          if [[ "${COMP_WORDS[0]}" == "tpm2_load" || "${COMP_WORDS[0]}" == "tpm2_loadexternal" ]]; then
            _filedir
          fi
          return;;
      -I)
          if [[ "${COMP_WORDS[0]}" != "tpm2_nvdefine" ]]; then
            _filedir
          fi
          return;;
      -k | -K)
          if [[ "${COMP_WORDS[0]}" == "tpm2_import" ]]; then
            _filedir
          fi
          return;;
      -i)
          if [[ "${COMP_WORDS[0]}" == "tpm2_send" ]]; then
            _filedir
          fi
          return;;
      -m)
          if [[ "${COMP_WORDS[0]}" != "tpm2_quote" ]]; then
            _filedir
          fi
          return;;
      -t)
          if [[ "${COMP_WORDS[0]}" == "tpm2_sign" ]]; then
            _filedir
          fi
          return;;
      -L)
          if [[ "${COMP_WORDS[0]}" == "tpm2_create" || "${COMP_WORDS[0]}" == "tpm2_createprimary" || "${COMP_WORDS[0]}" == "tpm2_nvdefine" ]]; then
            _filedir
          fi
          return;;
      -e)
          if [[ "${COMP_WORDS[0]}" == "tpm2_makecredential" ]]; then
            _filedir
          fi
          return;;

      -S | F | C)
         _filedir
         return;;

    esac
    $split && return

    local aux1=$( ${COMP_WORDS[0]} -h no-man 2>/dev/null )
    local aux2=$( echo "${aux1}" | tr "[]|" " " | awk '{if(NR>2)print}' | tr " " "\n" | sed 's/=<value>//')
    local suggestions=("${aux2[@]}" "${common_options[@]}") #generate all the opts for the tool

    if [[ "$cur" == -* ]]; then #start completion
        # exclude the already completed options from the suggested completions
        # e.g. if -T is already provided, the system will offer you --tcti=, but no -T
        local len=$(($COMP_CWORD - 1))
        local i
        for ((i=1 ; i<=len; i++)) ; do
            local aux="${COMP_WORDS[$i]}"
            if [[ $aux == -* ]] ; then
                (( i<len )) && [[ ${COMP_WORDS[$(( i + 1))]} == '=' ]] && aux="$aux="
                suggestions=( "${suggestions[@]/$aux}" )
            fi
        done
        COMPREPLY=( $(compgen -W '$( echo ${suggestions[@]} )' -- "$cur") )
        [[ $COMPREPLY == *= ]] && compopt -o nospace
        return
    fi

    COMPREPLY=( $( compgen -W '$( echo ${suggestions[@]} )' -- "$cur" ) )
} &&
complete -F _tpm2_tools ${COMP_WORDS[0]##*/}

# ex: filetype=sh
