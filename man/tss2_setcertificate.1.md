% tss2_setcertificate(1) tpm2-tools | General Commands Manual
%
% APRIL 2019

# NAME

**tss2_setcertificate**(1) -

# SYNOPSIS

**tss2_setcertificate** [*OPTIONS*]

# DESCRIPTION

**tss2_setcertificate**(1) - This command associates an x509 certificate in PEM encoding into the path of a key.

# OPTIONS

These are the available options:

  * **-p**, **\--path**:

    Identifies the entity to be associated with the certificate. MUST NOT be NULL.

  * **-i**, **\--x509certData**:

    The PEM encoded certificate. MAY be NULL. If x509certData is NULL then the stored x509 certificate SHALL be removed.

[common tss2 options](common/tss2-options.md)

# EXAMPLE

tss2_setcertificate --path HS/SRK/myRSACrypt --x509certData certificate.file

# RETURNS

0 on success or 1 on failure.

[footer](common/footer.md)
