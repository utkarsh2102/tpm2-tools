# Testing Framework

The command **make check** can be used to run the test scripts.

The configure option `--enable-unit` must be specified and the
tpm2-abrmd and tpm_server must be found on PATH. If they are installed
in custom locations, specify or export PATH during configure.

For example:
```sh
./configure --enable-unit PATH=$PATH:/path/to/tpm2-abrmd:/path/to/tpm/sim/ibmtpm974/src
```

## Adding a new integration test
To add a new test, do:

1. add a script to the integration directory.
2. source helper.sh in the new script.
4. issue the command start_up.
5. Do whatever test you need to do.
6. If you set the EXIT handler, call tpm2_shutdown in that handler.
7. make distclean, re-run bootstrap and configure to pick up the new script.
8. Run make check again.