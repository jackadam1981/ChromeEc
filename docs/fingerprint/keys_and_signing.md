# Fingerprint Firmware Keys and Signing

[TOC]

## Keys

The `RO` section of the fingerprint firmware contains the public portion of the
key used to sign the RW firmware. It uses the public key to validate the
signature of the RW firmware before jumping to it. It is not possible to
update the public key stored in the RO firmware once a device has been shipped
(i.e., once the hardware write protect is enabled).

*** promo
TODO(tomhughes): Add details about different types of keys (`dev`, `premp`,
`mp`, etc).
***

### Resources

*   https://sites.google.com/a/google.com/chromeos/resources/engineering/releng/signer-documentation
*   https://sites.google.com/a/google.com/chromeos/paygen---payload
*   https://b.corp.google.com/issues/77882970

## Signing

[`futility`](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/master/futility/)
is used to sign EC firmware. There’s a wrapper script around it for signing
called
[`sign_official_build.sh`](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/master/scripts/image_signing/sign_official_build.sh).

### Key ID

The output of `futility show` will show a `Public Key File` and `Signature`
section, each of which have an `ID` field. This ID lets you match the key to the
signature in case there is more than one.
[It’s just a sha1sum of the public key,](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/e7db36856ce418552637d1981c173d22dfe5bf39/firmware/2lib/include/2id.h#5)
so it lets you
[uniquely identify the key being used](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/e7db36856ce418552637d1981c173d22dfe5bf39/firmware/2lib/include/2rsa.h#14).

If you have the key (e.g., in PEM format), you can compute the `ID` with the
`futility show` command:

```bash
(chroot) $ futility show ./path/to/key.pem
```

#### Example

If you are building the `hatch_fp` "board" on your local machine (which signs
the resulting `ec.bin` with the `dev` key, you can check the `ID` with:

```bash
(chroot)$ futility show board/hatch_fp/dev_key.pem
```

```
Private Key file:      board/hatch_fp/dev_key.pem
  Key length:          3072
  Key sha1sum:         61382804da86b4156d666cc9a976088f8b647d44
```

```bash
(chroot)$ futility show build/hatch_fp/ec.bin
```

```
Public Key file:       build/hatch_fp/ec.bin
  Vboot API:           2.1
  Desc:                ""
  Signature Algorithm: 7 RSA3072EXP3
  Hash Algorithm:      2 SHA256
  Version:             0x00000001
  ID:                  61382804da86b4156d666cc9a976088f8b647d44
Signature:             build/hatch_fp/ec.bin
  Vboot API:           2.1
  Desc:                ""
  Signature Algorithm: 7 RSA3072EXP3
  Hash Algorithm:      2 SHA256
  Total size:          0x1b8 (440)
  ID:                  61382804da86b4156d666cc9a976088f8b647d44
  Data size:           0x2864c (165452)
Signature verification succeeded.
```

### Showing Key ID (fingerprint) for running FW

[Asked on chromeos-chatty-firmware](https://groups.google.com/a/google.com/d/msg/chromeos-chatty-firmware/ZSg423wsFPg/26UbdGwjFQAJ)
about adding an EC command to show the Key ID (fingerprint) from the RO version.
This would make it a lot easier during both development and testing.
