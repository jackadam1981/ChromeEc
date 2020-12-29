# No Chroot Setup

The majority of the EC userbase developes/compiles the Chromium EC project with
the support of the [Chromium OS chroot][building-chromium-os] environment.

This environment provides compilers, tools, and other source components for
building Chromium OS and Chromium EC, but the Chromium EC project doesn't
require it.

This tutorial will demonstrate how to build Chromium EC in a normal GNU/Linux
environment.

[building-chromium-os]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/developer_guide.md#building-chromium-os

## Debian Package Requirement

```bash
sudo apt install binutils-arm-none-eabi gcc-arm-none-eabi
# Optional
sudo apt install libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib libnewlib-doc
```

## Source Setup

```bash
git clone https://chromium.googlesource.com/chromiumos/platform/ec
git clone https://chromium.googlesource.com/chromiumos/third_party/cryptoc
git clone https://chromium.googlesource.com/chromiumos/platform/vboot_reference
```

## Uploading Changes

*The rough outline is:*

1.  Commit the change
1.  Ensure that you have an account on https://chromium-review.googlesource.com
    and authentication is setup properly.
1.  Ensure that the Gerrit Change-Id hook is installed to the local repo.

    ```bash
    f=`git rev-parse --git-dir`/hooks/commit-msg ; mkdir -p $(dirname $f) ; curl -Lo $f https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x $f
    ```

1.  Upload to Gerrit for review

    ```bash
    git push origin HEAD:refs/for/main%r=cros-ec-reviewers@google.com
    ```

1.  Go to the link provided during the upload and then follow the
    [Code Reviews](code_reviews.md) instructions for how to request a Chromium
    EC reviewer.
