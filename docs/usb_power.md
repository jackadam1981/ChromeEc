# USB Power Considerations

Users want to be able to charge external devices using their Chromebook USB
ports, e.g. charge a phone from their Chromebook. We want to provide a fast
charging experience to end-users, so we prefer to offer high power charging when
possible.

[TOC]

## Summary of Design Requirements

For explanations of calculations see rest of doc.

### Total System Power

Total current needed for external USB devices at 5V:

```
(Number of Type-C Ports) * (1800mA) +
1500mA +
(Number of Type-A Ports) * (1500mA)
```

### Daughter Board Requirements

*   For 1A1C Daughter Boards, the DB ribbon cable needs to be able to
    [carry 4.8A](#db) of current a 5V to the DB.
    *   The 4.8A may all be on the PP5000 net or it may be split between PP5000
        and the VBus net. It depends on where the Type-C power path switches are
        (DB or MLB).
*   The ground path on the ribbon cable from the DB also needs to be able to
    carry enough current to match the power rails.

## USB Type-A Ports

For Type-A ports, the [BC 1.2 Specification] adds higher power modes on top of
the [USB 3.2 Specification]. While BC 1.2 support isn't required, it is
preferred, as it allows end-users to charge their devices more quickly.

[BC 1.2 Specification] defines multiple modes of operation including, but not
limited to:

*   CDP - Charging Downstream Port
    *   Allows USB Data. Provides guaranteed 1.5A @ 5V power.
    *   ChromeOS device can act as a CDP.
*   SDP - Standard Downstream Port
    *   Allows USB Data. Provides guaranteed current defined by USB
        Specifications
        *   For USB 4, provides guaranteed current of 1.5A @ 5V.
        *   For USB 3, provides guaranteed current of 0.9A @ 5V.
        *   For USB 2, provides guaranteed current of 0.5A @ 5V.
    *   ChromeOS device can act as a SDP.
*   DCP - Dedicated Charging Port
    *   No USB Data. Provides guaranteed of 1.5A @ 5V power.
    *   ChromeOS device **will not** act as a DCP.

For detection logic of each mode (e.g. on the D+ and D- pins) and nuance of
power/current power requirements, see full [BC 1.2 Specification].

Without BC 1.2 support, the max power requirements match that of a Standard
Downstream Port (SDP) as defined by various specification (e.g.
[USB 3.2 Specification]).

### ChromeOS as Source - Policy for Type-A

Since USB 4 bumps the minimum guaranteed current to 1.5A, ChromeOS will ensure
that all Type-A ports guarantee 1.5A of current for new designs.

Also, some peripherals may draw more current than the USB specification that
they are using for data, and we want to provide enough current for those Type-A
peripherals if we can, up to 1.5A.

Inserting a Type-C device does not affect the guaranteed power for a Type-A
port.

The total current needed for all Type-A ports at 5V is:

```
(Number of Type-A Ports)*(1500mA)
```

## USB Type-C Ports

USB Type-C allows for dynamic negotiation of high power contracts; this is
accomplished through varying CC resistor and/or USB-C Power Delivery (PD). More
in depth information can be found in the [USB PD Specification]; power contracts
can range from 0mA/3.3V to 5A/20V.

### ChromeOS as Source - Policy for Type-C

ChromeOS devices currently source power to external USB devices at 5V with a
typical current of 1.5A for each Type-C port (some older devices only source
900mA). In certain scenarios, a single Type-C port can source up to 3A @ 5V.

ChromeOS devices prefer that the first PD-capable Type-C device that is inserted
should get 3A guaranteed at 5V. When another PD-capable Type-C device is
inserted, then the original Type-C port will dynamically downgrade its current
limit from 3A to 1.5A.

Inserting a Type-A device does not affect the guaranteed power from a Type-C
port; only Type-C devices affect the power of Type-C ports.

For example, if a non-PD capable Type-C keyboard is inserted first, it will only
be guaranteed 1.5A, since it cannot PD negotiate. If the second inserted Type-C
device is a PD-capable phone, the the phone will be guaranteed 3A until another
PD-capable device is also inserted. When a second Type-C, PD-capable phone is
inserted, then both phones will only get a guaranteed 1.5A of current.

Type-C ports also need to provide an additional 300mA @ 5V (= 1.5W) for Vconn on
every port. Note: the 1.5W for Vconn may also be supplied via 455mA @ 3.3V
instead.

The total current needed for all Type-C ports at 5V is:

```
(Number of Type-C Ports) * (1500mA + 300mA) +
1500mA
```

The total maximum current needed for a single Type-C port at 5V is `(3000mA +
300mA) = 3.3A`. This matters for a daughter board ribbon cable.

## Daughter Board Considerations

{#db}If a daughter board has 1 Type-A and 1 Type-C, the the max USB device load
is

Scenario | Type-A Vbus | Type-C Vbus | Type-C Vconn | Total
-------- | ----------- | ----------- | ------------ | ------
1 Type-C | 0mA         | 3000mA      | 300mA        | 3300mA
1 Type-A | 1500mA      | 0mA         | 0mA          | 1500mA
Both     | 1500mA      | 3000mA      | 300mA        | 4800mA

The DB ribbon cable needs to be able to carry 4.8A of power to the DB. The 4.8A
may all be on the PP5000 net or it may be split between PP5000 and the VBus net.
It depends on where the Type-C power path switches are (DB or MLB).

The ground path on the ribbon cable from the DB also needs to be able to carry
enough current to match the power rails.

[BC 1.2 Specification]: <https://www.usb.org/document-library/battery-charging-v12-spec-and-adopters-agreement>
[USB 3.2 Specification]: <https://www.usb.org/document-library/usb-32-specification-released-september-22-2017-and-ecns>
[USB PD Specification]: https://www.usb.org/document-library/usb-power-delivery
