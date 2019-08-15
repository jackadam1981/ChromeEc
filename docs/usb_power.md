# USB Power Considerations

Users want to be able to charge external devices using their Chromebook USB
ports, e.g. charge a phone. We want to provide a fast charging experience to
end-users, so we prefer to offer high power charging when possible.

## Summary of Design Requirements

For explanations of calculations see rest of doc.

### Total System Power

Total power needed for external USB devices:

```
(# TypeC) * (1800mA) +
(# TypeA) * (900mA) +
600mA†
```

† Drop 600mA if TypeA does not support BC 1.2

### GPIO/I2C Signalling Requirements

        * TypeA ports must inform EC that port is in use and potentially drawing power
                * Typically implemented by GPIO from BC1.2 chip to EC
        * TypeC ports must inform EC that port is in use and potentially drawing power
                * Typically implemented by i2c communication between EC and TCPC

### Daughter Board Requirements

        * For 1A1C Daughter Boards, the DB ribbon cable needs to be able to [carry 3.3A](#db) of power to the DB.
                * The 3.3A may all be on the PP5000 net or it may be split between PP5000 and the VBus net. It depends on where the TypeC power path switches are (DB or MLB).
        * The ground path on the ribbon cable from the DB also needs to be able to carry 3.3A in addition to power need for ICs on DB.

## USB TypeA Ports

For TypeA ports, the [BC 1.2 Specification] adds higher power modes on top of
the [USB 3.2 Specification]. While BC 1.2 support isn't required, it is
preferred, as it allows end-users to charge their devices more quickly.

[BC 1.2 Specification] defines multiple modes of operation including, but not
limited to:

        * CDP - Charging Downstream Port
                * Allows USB Data. Provides max 1.5A @ 5V power.
                * ChromeOS device can act as a CDP.
        * SDP - Standard Downstream Port
                * Allows USB Data. Provides max of 900mA @ 5V power (USB 3.X).
                * ChromeOS device can act as a SDP.
        * DCP - Dedicated Charging Port
                * No USB Data. Provides max of 1.5A @ 5V power.
                * ChromeOS device *will not* act as a DCP.

For detection logic of each mode (on the D+ and D- pins) and nuance of
power/current power requirements. See full [BC 1.2 Specification].

Without BC 1.2 support, the max power requirements match that of a Standard
Downstream Port (SDP) as defined by the [USB 3.2 Specification], namely 900mA
for Gen 3.X and 500mA for Gen 2.X -- both at 5V.

### ChromeOS Policy for TypeA

If BC 1.2 is supported for a ChromeOS device, then the first TypeA port in use
will act as a CDP, providing a maximum current of 1.5A while also enabling USB
data. All other TypeA ports will only be SDP, providing a maximum current of
900mA.

Note that the CDP TypeA port allocation is dynamic; the first TypeA port in use
claims the higher, 1.5A current, and then all other TypeA ports get downgraded
to the lower, 900mA current.

The allocation of the one CDP TypeA port is unaffected by user interaction with
TypeC ports. Once a TypeA port has been claimed as CDP, inserting a TypeC device
will not revoke the CDP status of the TypeA port.

Once the last TypeA device has been removed, then any TypeA port can become the
one CDP TypeA port; CDP is claimed again by the first TypeA port in use after
all TypeA ports have been vacant.

The total current needed for all TypeA ports at 5V is:

```
if (BC1.2_Supported)
    (# TypeA Ports)*(900mA) + 600mA
else
    (# TypeA Ports)*(900mA)
```

## USB TypeC Ports

TypeC allows for dynamic negotiation of high power contracts; this is
accomplished through USB-C Power Delivery (PD) and more in depth information can
be found in the [USB PD Specification]. Power contracts can range from 500mA/5V
to 5A/20V.

### ChromeOS Policy for TypeC

ChromeOS devices currently source power to external USB devices at 5V with a
minimum current of 1.5A for each TypeC port. In certain scenarios, a single
TypeC port can source up to 3A @ 5V.

ChromeOS devices prefer that if *one and only one* USB port (including TypeA and
TypeC) is in use, and that single port is TypeC, then it should source 3A at 5V.
When another USB device is inserted, either TypeA or TypeC, then the original
TypeC port will dynamically downgrade its current limit from 3A to 1.5A.

TypeC ports also need to provide an additional 300mA @ 5V for Vconn on every
port.

The total current needed for all TypeC ports at 5V is:

```
(# TypeC Ports)*(1500mA + 300mA)
```

The total maximum current needed for a single TypeC port at 5V is `(3000mA +
300mA) = 3.3A`. This matters for a daughter board ribbon cable.

## Daughter Board Considerations

{#db}If a daughter board has 1 TypeA and 1 TypeC, the the max USB device load is

Scenario | TypeA Vbus | TypeC Vbus | TypeC Vconn | Total
-------- | ---------- | ---------- | ----------- | --------
1 TypeC  | 0mA        | 3000mA     | 300mA       | 3300mA
1 TypeA  | 1500mA     | 0mA        | 0mA         | 1500mA
Both     | 1500mA     | 1500mA**   | 300mA       | 3300mA**

** Current hardware designs do not honor the policy that inserting a TypeA
device will downgrade the TypeC device from 3A to 1.5A, so the maximum current
needed for that topology is actually 4800mA.

The DB ribbon cable needs to be able to carry 3.3A of power to the DB. The 3.3A
may all be on the PP5000 net or it may be split between PP5000 and the VBus net.
It depends on where the TypeC power path switches are (on the DB or MLB).

The ground path on the ribbon cable from the DB also needs to be able to carry
3.3A in addition to power need for ICs on DB.

[BC 1.2 Specification]: <https://www.usb.org/document-library/battery-charging-v12-spec-and-adopters-agreement>
[USB 3.2 Specification]: <https://www.usb.org/document-library/usb-32-specification-released-september-22-2017-and-ecns>
[USB PD Specification]: https://www.usb.org/document-library/usb-power-delivery
