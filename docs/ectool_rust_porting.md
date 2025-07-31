# ECTool How to port to rust

## Overview

This is a guide on how to port any changes to ectool or host commands over to
the rust variant of ectool.

## Porting Host command definitions

Host commands are defined in [`ec_commands.h`], this file is copied over the
rust implementation and parsed by rust's bindgen module to create rust bindings.
Rust's bindgen struggles to parse structs with zero length arrays and C unions.
So its recommended to manually create the rust version of the request/response
structures.  Take a look at the following simple example of porting
`EC_CMD_HELLO`, and we can go into more detail of each line afterwards.


**C Implementation**
```c
/*
 * Hello.  This is a simple command to test the EC is responsive to
 * commands.
 */
#define EC_CMD_HELLO 0x0001

/**
 * struct ec_params_hello - Parameters to the hello command.
 * @in_data: Pass anything here.
 */
struct ec_params_hello {
	uint32_t in_data;
} __ec_align4;

/**
 * struct ec_response_hello - Response to the hello command.
 * @out_data: Output will be in_data + 0x01020304.
 */
struct ec_response_hello {
	uint32_t out_data;
} __ec_align4;
```

**Rust Implementation**
```rust
/// EC host command Hello (`EC_CMD_HELLO`)
pub mod hello {
    use crate::commands::{EcCommandCode, EcCommandRequest, EcCommandVersion};
    use ec_commands_bindgen::EC_CMD_HELLO;
    use zerocopy::{FromBytes, Immutable, IntoBytes};

    /// The EC will respond with in_data + 0x01020304
    pub const EC_CMD_HELLO_RESPONSE: u32 = 0x01020304;

    /// EC host command Hello request (params)
    #[derive(Immutable, IntoBytes)]
    #[repr(C)]
    pub struct Request {
        /// Output will be in_data + 0x01020304
        pub in_data: u32,
    }

    /// EC host command Hello response
    #[derive(FromBytes)]
    #[repr(C)]
    pub struct Response {
        /// Output will be in_data + 0x01020304
        pub out_data: u32,
    }

    impl EcCommandRequest for Request {
        const CMD_CODE: EcCommandCode = EcCommandCode(EC_CMD_HELLO as u16);

        const VERSION: EcCommandVersion = EcCommandVersion(0);

        type Response = Response;
    }
}
```

There are a few items to look at here.

```rust
    use crate::commands::{EcCommandCode, EcCommandRequest, EcCommandVersion};
```
These are the generic types used for defining the Request to be sent to the EC.
The rust `use` keyword is kinda like C's `#include`

```rust
    use ec_commands_bindgen::EC_CMD_HELLO;
```
This is "including" rusts bindgen to parsed value from `ec_commands.h` so we
can use it in this module.

```rust
    use zerocopy::{FromBytes, Immutable, IntoBytes};
```
[zerocopy] are traits that we can add to the request and response structure
definitions, that help convert it to byte sequences to send over the wire.
You'll notice these are used prior to structure definitions in the
`#[derive(...)]` syntax.


```rust
    /// EC host command Hello request (params)
    #[derive(Immutable, IntoBytes)]
    #[repr(C)]
    pub struct Request {
        /// Output will be in_data + 0x01020304
        pub in_data: u32,
    }
```
Here we are defining the request structure for Hello command.  Since this is a
request, we want it `Immutable` and convertible `IntoBytes` so those are the
derived traits.  We also want it to represent the C structure, so `#repr(C)` is
used. When dealing with unaligned data, you may need to use `#repr(C, packed)`.

```rust
    /// EC host command Hello response
    #[derive(FromBytes)]
    #[repr(C)]
    pub struct Response {
        /// Output will be in_data + 0x01020304
        pub out_data: u32,
    }
```

The response will be read as a byte sequence from the EC, so we use the
`FromBytes` trait to convert it to this structure.

```rust
    impl EcCommandRequest for Request {
        const CMD_CODE: EcCommandCode = EcCommandCode(EC_CMD_HELLO as u16);

        const VERSION: EcCommandVersion = EcCommandVersion(0);

        type Response = Response;
    }
```
This is the unique part of the rust ectool implementation.  This implements the
`EcCommandRequest` for the `pub struct Request` defined earlier.  This is
mapping the `EcCommandCode`, `EcCommandVersion`, and `Response` to the
`Request` structure.  This is used to the `exec_command` to convert byte
sequences to be sent over the wire, and reading back the response and converting
it to the `Response` type.

## Porting Command Line Interface
Rust is leveraging [CLAP] module to handle command line arguments. This simply
converts rust enums into command line arguments. Look over the CLI in
[`ectool.cc`] and see what arguments are use for the command.

Continuing from the earlier `EC_CMD_HELLO` example,

**C Implementation**
```c
int cmd_hello(int argc, char *argv[])
{
	struct ec_params_hello p;
	struct ec_response_hello r;
	int rv;

	p.in_data = 0xa0b0c0d0;

	rv = ec_command(EC_CMD_HELLO, 0, &p, sizeof(p), &r, sizeof(r));
	if (rv < 0)
		return rv;

	if (r.out_data != 0xa1b2c3d4) {
		fprintf(stderr, "Expected response 0x%08x, got 0x%08x\n",
			0xa1b2c3d4, r.out_data);
		return -1;
	}

	printf("EC says hello!\n");
	return 0;
}
```

**Rust Implementation**
From `system/desktop/ec/src/ectool/cli.rs` there's an high level `Commands` enum
that handles the base `ectool <command> <arg1>..<argN>`.

```rust
#[derive(Subcommand, Debug)]
#[clap(rename_all = "lower")] // Match ChromeOS ectool command name format.
pub enum Commands {
    /// Turn on Automatic fan speed control
    AutoFanCtrl(AutoFanCtrlArgs),
    /// Manually force base state to attached, detached or reset. Only applicable for devices with
    /// detachable bases.
    BaseState(BaseStateArgs),
    :
    Hello
    :
}
```
Using the `#[derive(Subcommand)]` trait autogenerates the code to convert
strings to a `enum Commands` type. As `EC_CMD_HELLO` doesn't accept any
arguments, there's no `Args` structure as part of the enum.

The command is then handled in `system/desktop/ec/src/ectool/ectool.rs` via a
`match` statement (which is equivalent to a C `switch` case),

```rust
    match &cli.command {
        cli::Commands::AutoFanCtrl(auto_fan_ctrl_args) => {
            cmd_auto_fan_ctrl(&ec, auto_fan_ctrl_args)
        }
	:

        cli::Commands::Hello => cmd_hello(&ec),
```

and creates the request and sends it off to EC to be handled.

```rust
fn cmd_hello(ec: &EcDevice) -> Result<()> {
    const CMD_HELLO_DATA: u32 = 0xa0b0c0d0;
    let request = hello::Request { in_data: CMD_HELLO_DATA };
    let mut command = EcCommand::new(request);

    ec.exec_command(&mut command)?;

    // EC should return |params.in_data + EC_CMD_HELLO_RESPONSE|.
    let expected = CMD_HELLO_DATA + hello::EC_CMD_HELLO_RESPONSE;
    if command.response.out_data != expected {
        bail!("Expected: {0:#010x}, Received: {1:#010x}", expected, command.response.out_data);
    }

    println!("EC says hello!");
    Ok(())
}
```

The `ec.exec_command(...)` is a blocking function. It will
 1. Convert the `command` into the `Request` byte sequence and sends it to EC.
 2. Wait for EC to send its response byte sequence.
 3. Convert the byte response to the `Response` structure.
 4. Returns number of bytes read from EC or EC error code response (negative
    value).

The trailing `?` at the end of a function call, parses the return `Result<()>`
value of a function call.  If it's an `Error` type, the function will dump the
error to the terminal and halt execution.  This is similar to catching
exceptions in C++.  If the `Result<()>` return value is `Ok(...)`, the `?`
unwraps the return value.

[CLAP]:https://docs.rs/clap/latest/clap/
[`ec_commands.h`]:https://chromium.googlesource.com/chromiumos/platform/ec/+/main/include/ec_commands.h
[`ectool.cc`]:
    https://chromium.googlesource.com/chromiumos/platform/ec/+/main/util/ectool.cc
[zerocopy]: https://docs.rs/zerocopy/latest/zerocopy/
