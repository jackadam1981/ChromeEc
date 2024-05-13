# Realtek RTS54xx Message Dissector for Wireshark

[wireshark and tshark invocations are interchangeable here]

This directory provides a wireshark plugin for decoding UCSI and vendor
communication messages with the RTS54xx PDC chip as described in the
`Realtek Power Delivery Command Interface` spec.

There are two ways to use this plugin:

## directly using the wireshark plugin provided by this directory:

`wireshark -X lua_script:.../rts54.lua -r .../file.pcap`

replace `...` with relative or absolute paths as necessary.

## installing the wireshark plugin in a user's plugins directory:

The -X flag can be avoided by installing the relevant code in the
wireshark plugins directory hierarchy. Copy `*.inc` and `*.lua` to a
dedicated directory in the wireshark plugins directory like:

`$HOME/.config/wireshark/plugins/rts54`

or, simply simlink to this directory:

`ln -snT $CWD $HOME/.config/wireshark/plugins/rts54`

This simplifies the wireshark invocation to:

`wireshark -r .../file.pcap`

**Note** that wireshark automatically loads files in the plugins directory.
This makes the use of the `-X` flag mutually exclusive with installing
the same files in the plugins directory.

## running wireshark on PDC unit test results:

run unit test:

`./twister -T zephyr/test/pdc`

examine message exchange:

`tshark -X lua_script:rts54.lua -r $EC/twister-out/native_posix/pdc.generic/rts.pcap`

produces this sample output:

```
    1   0.000000           EC → PDC          UCSI-RTS54xx 10 Connector Reset
    2   0.130000           EC → PDC          UCSI-RTS54xx 10 Get RTK Status
    3   0.250000          PDC → EC           UCSI-RTS54xx 8 Response-Get-RTK-Status
    4   0.260000           EC → PDC          UCSI-RTS54xx 10 Get Cable Property
    5   0.370000          PDC → EC           UCSI-RTS54xx 11 Response-Get-Cable-Property
    6   0.390000           EC → PDC          UCSI-RTS54xx 9 Get Capabilities
    7   0.490000          PDC → EC           UCSI-RTS54xx 22 Response-Get-Capabilities
    8   0.900000           EC → PDC          UCSI-RTS54xx 10 Get Connector Capability
    9   1.000000          PDC → EC           UCSI-RTS54xx 10 Response-Get-Connector-Capability
...
```

similarly,

`wireshark -X lua_script:rts54.lua -r $EC/twister-out/native_posix/pdc.generic/rts.pcap`

produdes this [screenshot](./wireshark-rts54-screenshot-1.png).


# Implementation [notes](./rts54-notes.md)
