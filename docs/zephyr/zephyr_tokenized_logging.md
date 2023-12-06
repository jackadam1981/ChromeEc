# Zephyr Tokenized Logging

[TOC]

## Introduction

Tokenized logging is a feature that reduces your binary's image size by
converting log format strings into 32 bit token identifiers. These format
strings and tokens are saved off into a database used for detokenizing when
viewing the logs. Zephyr EC leverages [Pigweeds Tokenizer](https://pigweed.dev/pw_tokenizer/)
module to accomplish tokenizing and detokenizing of logs.

## Enabling Tokenization in EC

Enable Kconfig `CONFIG_PLATFORM_EC_LOG_TOKENIZED` and its dependencies for the
board you want to enable tokenized logging.  Additionally, make sure `picolibc`
and `pigweed` modules are added to your board and don't forget to regenerate
`all_targets.generated.bzl`.

*Note: Tokenized logging is only supported for Zephyr EC*

Example: https://crrev.com/c/5080188/8.
```
register_brox_project(
    project_name="brox",
    modules=["picolibc", "ec", "pigweed"],
)
```

## Generate Token Database

After enabling tokenization, build your board as normal `zmake build <board>`.

### Board specific database
The zmake will generate the board's specific database at
`build/zephyr/<board>/output/database.bin`

### Unified database
zmake will also merge all database.bin's found under `build/zephyr` into a single
unified database which can be found at `build/tokens.bin`. This `tokens.bin` can
be used to detokenize logs for any board compiled with tokenizatinon enabled.

## Flashing EC

An argument has been added to the `util/flash_ec` script to update the token
database on DUT while flashing the EC.  Specify the IP address in the `dut_ip`
argument.

```
./util/flash_ec --board=markarth --zephyr --dut_ip=100.107.108.242
```

## Detokenizing Logs

EC logs are captured in two places.
* On DUT `/var/log/cros_ec.log` via [timberslide](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/timberslide)
* Over UART via [servod](https://chromium.googlesource.com/chromiumos/third_party/hdctools)

The token database needs to be referenced by each of these applications to
properly detokenize the logs.

### Timberslide

Timberslide expects the database to be located at one of these locations, and
will use the first found in this order.
1. `/usr/share/cros_ec/tokens.bin`
2. `/usr/local/usr/share/cros_ec/tokens.bin`
3. `/usr/local/cros_ec/tokens.bin`

The first two files are located on read-only partitiions. To manually update the
first location run the following

```
cros workon start chromeos-base/chromeos-zephyr -b <BOARD>
cros_sdk cros_workon_make --board=<BOARD> chromeos-base/chromeos-zephyr
cros deploy <DUT_IP> chromeos-base/chromeos-zephyr
```

Location 3 can be updated via secure copy
```
scp tokens.bin root@<DUT_IP>:/usr/local/cros_ec/tokens.bin
```

### Servod

servod accepts a `token_db` argument to the path of the token database. The default
path is `/usr/share/cros_ec/tokens.bin`.  Servod uses Pigweeds auto updating detokenizer,
this monitors the files for changes and reloads the database when changes occur.

#### Chroot Environment
```
sudo servod -b <BOARD> --token_db=/mnt/host/source/src/platform/ec/build/tokens.bin
```

#### Docker Environment
*As of 12/6/2023 servod docker is under dogfood so the following may change.*


Using docker image requires mounting your path to the token database to the docker image.
See [servod outside chroot](https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/main/docs/servod_outside_chroot.md#i-want-to-flash-firmware-how-do-i-do-that) for details.

You can mount your `ec/build` path to the docker image with the following command.
```
start-servod --channel=release --mount=<your_path>/ec/build:/tmp/firmware_to_flash -n flashing_servod -- --token_db=/tmp/firmware_to_flash/tokens.bin
```

Tokenization is defaulted on or off by the servod overlay configuration file.
Example: https://crrev.com/c/4981429.

Once connected to ec_uart_pty - you can specify the token database using above
```
%tokens on /tmp/firmware_to_flash/tokens.bin
```

The docker image should be prepopulated with a token database at
`/usr/share/cros_ec/tokens.bin`

Once servod is running you can toggle the detokenizer algorithm on or off. This
can be done by connecting to the `ec_uart_pty` and running one of the following.

```
%tokens on
%tokens on <path to token database>
%tokens off
```

Make sure to add the `%` character in the command, this is a special indicator to
EC3PO for OOBM commands.
Using `%tokens on` with no path reloads the last path specified.  On start-up this
will be `/usr/share/cros_ec/tokens.bin`.

Viewing logs on a tokenized EC with tokenization turned off will look like this.
```
23-12-06 14:44:37.794 ec:~> pd 0 state
pd 0 state
23-12-06 14:44:39.816 `o7eqFQAGBkVuYWJsZQNTTksDREZQ~`8RegCQA=~`P2J9PQxBdHRhY2hlZC5TTkuEwAQ=~`UpI03wxQRV9TTktfUmVhZHmCCA==~`ubjWdA==~`dwIKAA==~ec:~>
```
A failure to decode will dump its base64 tokenized message as well. You'll notice
above the base64 message is encapsulated with a prefix of `` ` `` (backtick) and
suffix `~`.

Note - depending on the terminal emulator used you may need to force a `\n`
character at the end of your command.  Some terminal emulators add `"\r\n"` when
enter is pressed. To force a "\n" when using socat press `<ctrl+v> <enter>` then
send it off with another press of enter. So the command will look something like
`%tokens off<ctrl+v><enter><enter>` using socat.

### Recovering from failures

A failure can occur when an outdated database is used with an EC image. Pigweed
provides a [Detokenizing CLI tool](https://pigweed.dev/pw_tokenizer/detokenization.html#detokenizing-cli-tool) to help with debugging detokenizing failures. Using the
tokenized message above, the below command detokenizes it to the following:

```
$ python3 ${PW_ROOT}/pw_tokenizer/py/pw_tokenizer/detokenize.py base64 -i failed.txt -p "\`" build/tokens.bin | sed "s/~//g"
Port C0 CC3, Enable - Role: SNK-DFP TC State: Attached.SNK, Flags: 0x9002 PE State: PE_SNK_Ready, Flags: 0x0201 SPR
ec:>
```

The `sed "s/~//g"` is used to strip the token suffix from the output.

## Historical Database Management

The historical token database is the database to support all boards and
its entire history of log format strings used over time.  This database should
handle all boards no matter when it was released.
This lives at `https://storage.googleapis.com/chromeos-localmirror/cros_ec/tokens/historical.bin`

Database management is handled in `recipes/build_firmware.py`.  The `firmware-zephyr-postsubmit`
will update the database on a daily basis. Race conditions are handled
using [request-preconditions](https://cloud.google.com/storage/docs/request-preconditions).

## Token Collisions -- TODO(b/287267896)

Upon CQ submission, LUCI will identify when collisions occur and notify the
developer to alter their log statement.
