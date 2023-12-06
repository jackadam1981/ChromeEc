# Zephyr Tokenized Logging

[TOC]

## Introduction

Tokenized logging is a feature that reduces your binary's image size by converting
log format strings into 32 bit token identifiers. These format strings and tokens
are saved off into a database used for detokenizing when viewing the logs.
Zephyr EC leverages pigweed's [tokenizer](https://pigweed.dev/pw_tokenizer/)
to accomplish tokenizing and detokenizing of logs.

## Enabling Tokenization in EC

Enable Kconfig `PLATFORM_EC_LOG_TOKENIZED` and its dependencies for the the board
you want to enable tokenized logging.  Additionally, make sure `picolibc` and
`pigweed` modules are added to your board.

```
register_brox_project(
    project_name="brox-tokenized",
    modules=["picolibc", "ec", "pigweed"],
)
```

## Generate Token Database

After enabling tokenization, build your board as normal `zmake build brox-tokenized`.
The zmake will generate the board's specific database at `build/zephyr/<board>/build-r(o|w)/database.bin`
zmake will also merge all database.bin's found under `build/zephyr` into a single
database which can be found at `build/tokens.bin`. This `tokens.bin` can be used
to detokenize logs for any board compiled with tokenizatinon enabled.

## Detokenizing Logs

EC logs are captured in two places.
* On DUT `/var/log/cros_ec.log` via `timberslide` [`platform2/timberslide`]
* Over UART via `servod` [`third_party/hdctools`]

The token database needs to be referenced by each of these applications to properly
detokenize the logs.

### Timberslide

Timberslide expects the database to be located at `/usr/share/cros_ec/tokens.bin`.
To manually update this run

```
cros workon start chromeos-base/chromeos-zephyr -b <BOARD>
cros deploy <DUT_IP> chromeos-base/chromeos-zephyr
```

### Servod

servod accepts a token_db argument to the path of the token database.

In chroot environment
```
sudo servod -b skyrim --token_db=/mnt/host/source/src/platform/ec/build/tokens.bin
```

*As of writing this servod docker is under dogfood so the following may change.*

Using docker image requires mounting your path to the token database to the docker image.
See [servod outside chroot](https://chromium.googlesource.com/chromiumos/third_party/hdctools/+/main/docs/servod_outside_chroot.md)
for details. The docker image should be prepopulated with a token database at
`/usr/share/cros_ec/tokens.bin`

Once servod is running you can toggle the detokenizer algorithm on or off. This
can be done by connecting to the `ec_uart_pty` and running one of the following.

```
%tokens on
%tokens on <path to token database>
%tokens off
```

Make sure to add the `%` character in the command, this is a special indiator to
EC3PO for OOBM commands.


Note - depending on the terminal emulator used you may need to force a `\n` character
at the end of your command.  Some terminal emulators add `"\r\n"` when enter is pressed.
To force a "\n" when using socat press `<ctrl+v> <enter>` then send it off with
another press of enter. So the command will look something like
`%tokens off<ctrl+v><enter><enter>` using socat.

## Historical Database Management -- WIP

The historical token database is the database to support all boards and
its entire history of log format strings used over time.  This database should handle
all boards no matter when it was released.
This lives at `gs://chromeos-localmirror/<TBD>`

1. Each builder creates database (db1, db2, db3, .etc)
2. A build job to merge all databases from each builder (db1, db2, db3, etc → unified.db)
3. Take unified.db and merge with latest historical.db → historical_new.db
4. Upload historical_new.db to GCS as historical.db (overwrite)
5. Release process fetch historical.db and load on DUT as /usr/share/cros_ec/tokens.bin
