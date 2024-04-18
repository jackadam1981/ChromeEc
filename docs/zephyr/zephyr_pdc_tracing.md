# Zephyr EC PDC Tracing

[TOC]

## Overview

PDC (PD controller) tracing can be enabled `CONFIG_USBC_PDC_TRACE_MSG`.
When enabled, the EC console `pdc trace` command or ectool equivalent
can be used to enable, disable and dump PDC messages.

These PDC messages are nominally standard UCSI messages, but in practice
are vendor specific. `pdc trace` can be used to examine messages
exchanges between the EC and the PDC chip.

## Examples

`pdc trace all` enables tracing between the EC and all PDC chips.
`pdc trace 0` enables tracing between the EC and all PDC chip #0.
`pdc trace off` disables tracing (the default).

`pdc trace` dumps the accumulated PDC messages. The output looks like:

```
SEQ:0004 PORT:1 OUT {
bytes 7: 08 05 9a 00 0a 01 03
}
```

Where `SEQ:wxyz` is the 16-bit message sequence number. This is added by
the tracing framework to help the consumer detect dropped messages.

`PORT:x` is the PDC port number associated with the messages.

`OUT` the direction of the message. Out means from the EC to the PDC.

`bytes n: ...` the number of bytes in the message followed by the actual
bytes.

## Implementation

PDC messages are captured by hooks in the PDC receive and transmit
routines. These are buffered in a fixed-size (e.g. 1 KiB) circular
buffer. When the buffer is full, no more messages are are accepted,
effectively shutting down tracing.

PDC messages are stored as `struct pdc_trace_msg_entry` (defined in
`ec_commands.h`) in the FIFO since that is the information needed by
the consumers of these messages.

PDC messages can be retrieved using a FIFO policy from this buffer.

In the simplest from the EC console command `pdc trace` can used to dump
these messages.

Alternatively, `ectool pdctrace` can be used to retrieve these messages.
`ectool` has additional functionality to write (or forward) these messages
to a PCAP file or a network destination where wireshark or tshark can be
used to examine PDC messages.
