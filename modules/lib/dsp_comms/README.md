# DSP comms
The communication protocol designed to communicate with the DSP core on Chromium
boards. Currently, the comms channel supports the following messages:
* EC notifies the DSP of a lid-open event (GMR signal forwarding)
* EC notifies the DSP of a table-mode event (GMR signal forwarding)
* DSP request CBI values from EC

## Data flow
2 types of communications are supported:
1. Client initiated
2. Server initiated

![Transport Flow]

### Client initiated
This flow starts when the client requires to either notify or fetch some data
from the server.

1. Client sends a request over the bus.
2. Server handles the request and serializes the response. That response is then
placed on a buffer along with a header (_N_ bytes) and a header size (1 byte).
The server then fires an interrupt to notify the client that there's a header
ready to read.
3. Client reads 1 byte which tells it the size of the header. If the server's
lifecycle outlasts the client then the client may memoize the size for future
responses. This size is guaranteed never to change while the server is running.
4. Client reads _N_ bytes for the header. The header is endiannss agnostic. It
contains 1 byte for the `response_length` and a _N-1_ long bitmask (`events`)
for event notification.
5. The client can broadcast events to any listeners and initiates a bus read for
`response_length` bytes.
6. The response is deserialized and used to satisfy the initial request.

### Server initiated
Server initiated communication begins when the server has to notify the client
of a new event. Since the client is the bus controller, it will have to initiate
the reads required to understand the event(s).

1. Server places an _N_ byte header and the 1 byte header size in the buffer
then fires the interrupt. The header contains a `response_length = 0` and one or
more bits in `events` modified compared to the last header.
2. Client reads 1 byte which tells it the size of the header. If the server's
lifecycle outlasts the client then the client may memoize the size for future
responses. This size is guaranteed never to change while the server is running.
3. Client reads _N_ bytes for the header. The header is endiannss agnostic. It
contains 1 byte for the `response_length` and a _N-1_ long bitmask (`events`)
for event notification.
4. The client can broadcast events to any listeners and since `response_length`
is `0` bytes, stops there.

## The server logic UML
![Service UML]

[Transport Flow]: transport_flow.png
[Service UML]: service_transport_uml.png
