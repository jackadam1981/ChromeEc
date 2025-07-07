# RST documentation for CrOS EC repo

## Installation guide
The only tool you need is bazelisk, see https://github.com/bazelbuild/bazelisk
for instructions.

## Building
```
bazelisk build //docs_rst:docs
```

## Opening
You can either to
`bazel-out/k8-fastbuild/bin/docs_rst/docs/_build/html/index.html` or use the
Bazel rule:
```
bazelisk run //docs_rst:open
```

## Getting help
https://technicalwriting.dev/sphinx/bazel/tutorial.html is a great place to
start learning how Bazel integrates with RST. The author is easy to reach for
any specific questions.
