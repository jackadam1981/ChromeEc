# Code Reviews
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


The `platform/ec` repository makes use of a code review system that tries to
evenly distribute code reviews among available reviewers.

[TOC]

## How to request a review
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


Add `cros-ec-reviewers@google.com` to the reviewer line in Gerrit. A background
job will come around and replace the `cros-ec-reviewers@google.com` address with
the next available reviewer in the EC reviewer rotation. This typically takes on
the order of minutes.

Optionally, you can click the [FIND OWNERS] button in the UI, and select
`cros-ec-reviewers@google.com`.

## When to use review system
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


If you are modifying code in `common/`, `chip/`, or `core/`, feel free to use
the `cros-ec-reviewers@google.com` system. It is **never** a requirement to use
`cros-ec-reviewers@google.com`. You can always request a review from a specific
person.

## Responsibilities of reviewers
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


If the selected reviewer is unfamiliar with code in a CL, then that reviewer
should at least ensure that EC style and paradigms are being followed. Once EC
styles and paradigms are being followed, then the reviewer can give a +1 and add
the appropriate domain expert for that section of code.

Reviewers should try to give an initial response within 1 business day of
receiving a review request. Thereafter, they should try to respond to new
comments by the author within 1 business day.

## Review guidelines
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


Authors and reviewers should follow the Chrome OS firmware review
[guidelines][2] while publishing and reviewing code.

## How can I join the rotation?
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


Add your name to the [list of reviewers][1].

## Reference
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/docs/code_reviews.md
***


* [Chrome OS firmware review guidelines][2]
* [Coreboot Gerrit Guidelines][3]
* [Google small CL guidelines][5]

[1]: http://google3/chrome/crosinfra/gwsq/ec_reviewers
[2]: http://chromium.googlesource.com/chromiumos/docs/+/master/firmware_code_reviews.md
[3]: https://doc.coreboot.org/getting_started/gerrit_guidelines.html
[5]: https://google.github.io/eng-practices/review/developer/small-cls.html
