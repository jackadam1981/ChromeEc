This directory contains the out-of-tree i2c-pseudo Linux kernel module.

Status of sending this module for upstream consideration is tracked in:
[https://issuetracker.google.com/129565355]

Automated installation:
```
$ ./install
```

For more information and usage details see:
* [Documentation/i2c/i2c-pseudo.rst] or [i2c-pseudo.md]
* [include/uapi/linux/i2c-pseudo.h]

The reStructuredText (.rst) file is
Documentation/i2c/pseudo-controller-interface.rst in the upstream patch.  The
Markdown file (.md) is generated using rst2md from
nb2plots (https://github.com/matthew-brett/nb2plots) which uses
Sphinx (https://www.sphinx-doc.org/).

The `i2c-pseudo.md` Markdown file is generated from
`Documentation/i2c/i2c-pseudo.rst` reStructuredText file using `rst2md` from
nb2plots (https://github.com/matthew-brett/nb2plots) which uses
Sphinx (https://www.sphinx-doc.org/).  Please regenerate the Markdown file when
changing the original reStructuredText file by running:
```
$ rst2md Documentation/i2c/i2c-pseudo.rst i2c-pseudo.md
```
