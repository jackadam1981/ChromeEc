# PDC Dead Battery Two Ports

[Return to PDC documentation](../pdc.md)

Upon initialization in dead battery scenarios, the PDC only negotiates 5V. This
protects the system from reverse current appearing on Type-C chargers when two
or more chargers are connected.  When EC selects the "best" charger in dead
battery scenarios, EC needs to disable sink paths on the non-preferred chargers
first to prevent reverse current before requesting more power from the "best" charger.
This is illustrated in the call flow below.
![PDC Dead Battery Two Ports](pdc_dead_battery_two_ports.png)

Dead battery scenarios the EC handles
| Num PDC ports sinking | Battery status | Expectation |
| --------------------- | -------------- | ----------- |
| 1                     |  Present       | Select best PDO from charger |
| 1                     |  Not present   | Prevent changing RDO as PDC during negotiation reduces power to pSnkStby (2.5W) causing system to brown out. |
| > 1                   |  Present       | Prevent reverse current appearing on Type-C chargers |
| > 1                   |  Not present   | All the above|
