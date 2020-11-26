# WSServer a C WebSocket Server

[![Build Status](https://travis-ci.org/mortzdk/Websocket.svg?branch=master)](https://travis-ci.org/mortzdk/Websocket) [![Financial Contributors](https://opencollective.com/websocket/tiers/badge.svg)](https://opencollective.com/websocket/) 

WSServer is a fast, configurable, and extendable WebSocket Server for UNIX
systems written in C (C11).

As of version 2.0.0 the WSServer has been completely rewritten with many new
features, better support, better extendability and generally as a more stable
WebSocket server.

Current Version: **v2.0.6**

### Early history

The original WebSocket server (v1.0.0 and before) started out as a hobby
project in 2012. The idea at the time, was to learn the C language and
understand the basics of WebSockets. The initial version of the server worked
for some but not all aspects of the protocols present at that time. At that
time, different browsers implemented different versions of the WebSocket
Protocol, which made it difficult to support all browsers. Today all major
browsers support the [RFC6455](http://tools.ietf.org/html/rfc6455) protocol,
which has been stable for many years. 

### Present (2020)

WSServer now supports all aspects of the [RFC6455](http://tools.ietf.org/html/rfc6455) protocol, including the
[RFC7692](https://tools.ietf.org/html/rfc7692) that enables the `permessage-deflate` extension. The 
implementation is verified by the [Autobahn testsuite](https://github.com/crossbario/autobahn-testsuite) and by a lot of
unit tests. It furthermore support the older protocols [HYBI10](https://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-10) and [HYBI07](https://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-07)
as there are no noteable difference between those and the current stable one.
Currently there are no support for older versions of the protocol.

### Donate

To support further development on this project and financially support the 
developer having a nice cup of coffee, you can make a donation of your choice
[here](https://opencollective.com/websocket).

# How to get started

To build the WSServer for your UNIX system, simply run: `make release`. This
will compile extensions, subprotocols, and the binary that will be available
from: `./bin/WSServer`.

Currently the WSServer support only one extension namely the `permessage-deflate`
extension. Read more about this implementation [here](#Permessage-Deflate).

Furthermore it supports two subprotocols: [echo](#Echo) and [broadcast](#Broadcast). 
The [echo](#Echo) subprotocol is a simple protocol that sends whatever message
received, back to the same client. This is also the default protocol chosen,
if no subprotocol is provided by the client. The [broadcast](#Broadcast) subprotocol
is slightly more advanced. It sends a message from one client to all other
connected clients. The behaviour is basically as a public chat room.

The server can be configured by providing a `-c [path_to_config_file.json]`
flag. If no configuration is provided, the server will run with a default
configuration, that support WebSocket over HTTP on port 80. You can read more
about the structure and description of the configuration file in the
[configuration](#Configuration) section.

### Log

WSServer keeps a log file at `logs/WSServer.log`. The detail of the log is
defined by the `log_level` value in the configuration. Default for a release
build is 3 (FATAL, ERROR, WARN, INFO). Default for a test build is 5 (FATAL,
ERROR, WARN, INFO, DEBUG, TRACE).

The log file is a good tool for discovering misbehavior of the server, such as
when the server isn't able to start, since port 80 is already occupied by
another server instance.

### Dependencies

WSServer does in principle not rely on any third-party libraries in order to
serve as a WebSocket server. However if you want the complete feature set the
[**zlib**](https://zlib.net/) and [**openssl**](https://www.openssl.org/) library must be installed on your system in order to
support the `permessage-deflate` extension and SSL (WSS).

##### Ubuntu
```
$ sudo apt-get install zlib1g-dev
$ sudo apt-get install openssl
```

##### Arch
```
$ pacman -S openssl
$ pacman -S zlib
```

##### MacOS
```
$ brew install openssl
$ brew install zlib
```

##### FreeBSD
```
$ pkg install openssl
$ pkg install zlib
```

No other dependencies are required with regards to building the server with the
full feature set.

If you want to run the [Autobahn testsuite](https://github.com/crossbario/autobahn-testsuite) and the unit tests yourself, further
dependencies are required. These are [**docker**](https://www.docker.com/) and [**criterion**](https://github.com/Snaipe/Criterion).

##### Ubuntu
```
$ sudo apt install docker.io
$ sudo add-apt-repository ppa:snaipewastaken/ppa
$ sudo apt-get update
$ sudo apt-get install criterion-dev
```

##### Arch
```
$ pacman -S docker
$ pacman -S criterion
```

##### MacOS
```
$ brew install docker
$ brew install snaipe/soft/criterion
```

##### FreeBSD
```
$ pkg install docker
```

Criterion can be build for FreeBSD using the following [guide](https://criterion.readthedocs.io/en/latest/setup.html#installation).  

### Configuration

An example of a configuration file can be found at [here](https://github.com/mortzdk/Websocket/blob/master/conf/wss.json). A lot of different
things are configurable through the configuration file. 

##### Origins

The `origins` key define a subset of allowed origins. It is always recommended
to define this subset. In case no subset is defined a client can connect from
anywhere.

##### WebSocket URI

A WebSocket URI consists of a *scheme*, *host*, *port*, *path* and a *query*.
Take for example: 

**wss://mortz.dk:9011/websocket?csrf-token=asgjh48hs389hdla**.

The **wss** part of the URI is defined as the *scheme*, the **mortz.dk** part
is defined as the host, the **9011** part is defined as the *port*, the
**/websocket** part is defined as the *path* and the
**?csrf-token=asgjh48hs389hdla** part is defined as the *query*.  The specific
allowance of all 5 parts of the WebSocket URI are configurable.

The `hosts` key define a subset of strings allowed as the host. In the example
above, if we only want clients connecting to **mortz.dk**, we can add that
string to the subset.

The `paths` key define a subset of strings allowed as the connecting path. In
the example above, if we only want clients connecting through the path
**/websocket**, we can add that string to the subset. The path **/** will
always be a valid connection path. The string values of the `paths` key are
allowed to use POSIX Extended Regular Expressions syntax.

The `queries` key define a subset of strings allowed as the queries. In the
example above, if we only want clients to use a specific query
__csrf-token=[^&]*__, we can add that string to the subset. The string values
of the `paths` key are allowed to use POSIX Extended Regular Expressions
syntax. A WebSocket URI without any queries is always allowed.

The *scheme* and *port* part of the WebSocket URI is checked based on the ports
choosen for `http` and `https`.

##### Port

The `port` key of the `setup` object is used to define the ports that http (ws)
version and the https (wss) should be listening to.

A http (ws) version of the server will always be available. The https (wss)
version requires further configuration of SSL.

##### Extensions

The `extensions` key of the `setup` object is used to define an array of
supported extensions. Each entry in the array is an object itself containing 
a `file` and `config` key. The `file` key should point to the location of the
shared object representing the extension. The basename of the `file` key is 
used as the extension name. The `config` key can be used to provide extra
configuration to the extension.

##### Subprotocols

The `subprotocols` key of the `setup` object is used to define an array of
supported subprotocols. Each entry in the array is an object itself containing 
a `file` and `config` key. The `file` key should point to the location of the
shared object representing the subprotocol. The basename of the `file` key is 
used as the subprotocol name. The `config` key can be used to provide extra
configuration to the subprotocol. 

##### Favicon

The `favicon` key of the `setup` object is used to define a favicon to serve
for HTTP and HTTPS request. A lot of browsers do the request for favicons per
default when performing HTTP and HTTPS requests. By defining this with the path
to a valid ICO file, the WSServer will return a favicon.

##### Timeouts

The `timeout` key of the `setup` object is used to define different timeouts of
the WSServer.

The `poll` key define a timeout for event polling. By setting it to a positive
integer **n**, the event loop will be interrupted every **n** milliseconds.

The `read` key define a timeout for the READ event. The timeout is checked
whenever the server requires to read more data from the client in order to
succeed. By setting it to a positive integer **n** the request will fail if the
next read took longer than **n** milliseconds.

The `write` key define a timeout for the WRITE event. The timeout is checked
whenever the server requires to write more data to the client than it is
currently able to buffer. By setting it to a positive integer **n**
the request will fail if the next write took longer than **n** milliseconds.

The `client` key define a timeout for when the client was last active. By
setting it to a positive **n** integer, the client will be disconnected if it
has not been active the last **n** milliseconds.

The `pings` key defines the amount of pings performed within the span of the
`client` timeout key. If this value is set, it is recommended to use a value
stricly higher than 1, as the internal timing of the server is not 100%
accurate.

##### Size

A lot of different sizes can be adjusted for the WSServer. All sizes but the
`ringbuffer` are defined in bytes.

The `payload` size define how large a size of payload the server is willing to
accept from the client.

The `header` size define how large a HTTP header the server will accept from
the client.

The `uri` size define how large a URI the server will accept from the client.

The `buffer` size define how large the internal read and write buffers should
be. 

The `thread` size define how large each thread of the WSServer can maximally be.

The `ringbuffer` size define how many messages about to be written each client
can store in their ringbuffer.

The `frame` size define the maximal payload size of a single frame.

The `fragmented` size define how many fragments (frames) one single message can
consist of.

##### Pool

Internally the WSServer runs a threadpool to schedule IO work from the clients.

The `worker` key define the amount of threads the threadpool shall consist of.
Generally the rule of thumb is that the higher the load, the more threads.
However the optimal amount of workers is probably system and hardware dependent.

##### SSL (WSS)

WSServer supports the *wss* scheme by the use of the OpenSSL library. In order
to activate SSL some configuration must be made.

The `key` key define the path to the SSL private key of the server. The private
key must be in the PEM format.

The `cert` key define the path to the SSL server certificate. The certificate
must be in the PEM format.

The `ca_file` key define the path to the root CA certificate.

The `ca_path` key define the path to a folder containing the trusted root CA
certifates.

The `dhparam` key define the path to the dhparam file. The dhparam file must be
in the PEM format.

The `cipher_list` key define the ciphers that the server allows usage of.

The `cipher_suites` key define the cipher suites that the server allows usage
of.

The `compression` key define whether compression should be used when
communicating over SSL,

The `peer_cert` key define whether a peer certificate is required by the
client.

# WebSocket Extensions

The WSServer enables usage of an arbitrary number of extensions. Extensions
provide a mechanism for implementations to opt-in to additional protocol
features.

The extensions themselves can be implemented in any language that is able to
compile into a shared object (*.so* file) with the following public
functions:

```
typedef void (*setAllocators)(WSS_malloc_t extmalloc, WSS_realloc_t extrealloc, WSS_free_t extfree);
typedef void (*onInit)(char *config);
typedef void (*onOpen)(int fd, char *param, char **accepted, bool *valid);
typedef void (*inFrame)(int fd, wss_frame_t *frame);
typedef void (*inFrames)(int fd, wss_frame_t **frames, size_t len);
typedef void (*outFrame)(int fd, wss_frame_t *frame);
typedef void (*outFrames)(int fd, wss_frame_t **frames, size_t len);
typedef void (*onClose)(int fd);
typedef void (*onDestroy)();
```

Where `WSS_malloc_t`, `WSS_realloc_t`, and `WSS_free_t` are defined
as:

```
typedef void *(*WSS_malloc_t)(size_t size);
typedef void *(*WSS_realloc_t)(void *ptr, size_t size);
typedef void (*WSS_free_t)(void *ptr);
```

and `wss_frame_t` is defined as:

```
typedef struct {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    uint8_t opcode;
    bool mask;
    uint64_t payloadLength;
    char maskingKey[4];
    char *payload;
    uint64_t extensionDataLength;
    uint64_t applicationDataLength;
} wss_frame_t;
```

For the server to be able to use a custom extension one has to configure the
path to the shared object in the configuration file as described [above](#Extensions).

You can have a look at the [extensions](https://github.com/mortzdk/websocket/blob/master/extensions) folder to see how to
implement your own extension.

### Permessage-deflate

The WSServer comes with 1 build-in extension, namely the `permessage-deflate`
extension defined in [RFC7692](https://tools.ietf.org/html/rfc7692). This
extension enables compression and decompression of the frames between client
and server.

# WebSocket Subprotocols

The WSServer also enables usage of an arbitrary number of subprotocols.
Subprotocols are application-level protocol layered over the WebSocket Protocol
that are used to define the behaviour of the websocket protocol.

The subprotocols themselves can be implemented in any language that is able to
compile into a shared object (*.so* file) with the following public
functions:

```
typedef void (*setAllocators)(WSS_malloc_t submalloc, WSS_realloc_t subrealloc, WSS_free_t subfree);
typedef void (*onInit)(char *config, WSS_send send);
typedef void (*onConnect)(int fd, char *ip, int port, char *path, char *cookies);
typedef void (*onMessage)(int fd, wss_opcode_t opcode, char *message, size_t message_length);
typedef void (*onWrite)(int fd, char *message, size_t message_length);
typedef void (*onClose)(int fd);
typedef void (*onDestroy)();
```

Where `WSS_send`, `WSS_malloc_t`, `WSS_realloc_t`, and `WSS_free_t` are defined
as:

```
typedef void (*WSS_send)(int fd, wss_opcode_t opcode, char *message, uint64_t message_length);
typedef void *(*WSS_malloc_t)(size_t size);
typedef void *(*WSS_realloc_t)(void *ptr, size_t size);
typedef void (*WSS_free_t)(void *ptr);
```

and `wss_opcode_t` is defined as:

```
typedef enum {
    CONTINUATION_FRAME = 0x0,
    TEXT_FRAME         = 0x1,
    BINARY_FRAME       = 0x2,
    CLOSE_FRAME        = 0x8,
    PING_FRAME         = 0x9,
    PONG_FRAME         = 0xA,
} wss_opcode_t;
```

You can have a look at the [subprotocols](https://github.com/mortzdk/websocket/blob/master/extensions) folder to see how to
implement your own subprotocol.

### Client Authentication

Client authentication is not implemented directly in WSServer, but is
supported through several means. The `onConnect` call of the subprotocol sends
information about the connection to the subprotocol, this is information such
as the *ip*, *port*, *path* and *cookies*. This can be used to do client
authentication using [cookies](https://coletiv.com/blog/using-websockets-with-cookie-based-authentication/), using query parameters of the path 
or simply by having an initial round of authentication messages between the
client and server.

As always it is strongly advised to use the [origins](#Origins) array of the
configuration to only allow for certain origins to use the server.

### Echo

The `echo` subprotocol is a very simple subprotocol that just echo's whatever
the client send back to the client. This subprotocol is especially useful for
testing and is used in the [Autobahn testsuite](https://github.com/crossbario/autobahn-testsuite).

### Broadcast

The `broadcast` subprotocol is slightly more advanced. It keeps track of when a
client is connecting or closing in order to hold a map of those clients that
should be broadcastet to. Whenever a client sends a message, the message is
broadcastet to all other connected clients.

# Documentation

WSServer automatically generates documentation based on the comments in the
code. This documentation can be viewed [here](https://mortzdk.github.io/Websocket/documentation/).

Furthermore one could take a look at the [RFC6455](http://tools.ietf.org/html/rfc6455) protocol and the
[RFC7692](https://tools.ietf.org/html/rfc7692) protocol to understand how WebSockets and the permessage-deflate
extension works.

# Testing

WSServer has been heavily tested by the use of unit tests, the 
[Autobahn testsuite](https://github.com/crossbario/autobahn-testsuite) and by having code coverage.

### Unit tests

A lot of unit tests has been written using the [Criterion](https://github.com/Snaipe/Criterion) unit testing library. As of right now the unit tests does not cover all files, and this is still work in progress.

The tests can be run by running `make test`.

### Autobahn Testsuite

The Autobahn Testsuite is used to verify that the WSServer complies to the 
[RFC6455](http://tools.ietf.org/html/rfc6455) protocol. These tests can be used
as both verification and as a measure of performance as a lot of the tests
actually times the execution of a successful test.

The tests can be run by running `make autobahn`.

You can further see the current results of the tests [here](https://mortzdk.github.io/Websocket/autobahn/).

### Code coverage

The coverage report can be generated by running `make test` and the latest can 
be seen [here](https://mortzdk.github.io/Websocket/gcov/).

# Further Work

Here is a list of prioritized further work that currently can be done:

1. Test on FreeBSD/MacOS
2. Use Autoconf to check dependencies
3. Rate limiting
    - Rate limiting connections
        - Count-Min Sketch (Sliding window)
            * Belongs to the server object
            * Allocate 60 count-min sketches (one per minute)
            * Reset sketch if rotated
    - Rate limiting messages
        - Can be done by the subprotocols per message?
    - Rate limiting frames
        - Counting using Sliding window
            * Belongs to the session object
            * Allocate 60 integers (one per minute)
            * Reset sketch if rotated
4. Fuzz testing
5. Support HTTP2
6. Performance Improvements
    - Look at 'khash' or 'tommy_hashdyn' instead of 'uthash' since these hashes
      seems to be faster
    - Realloc the double size of the current
    - Refactor `wss_frame_t` structure away. Use the frames as byte strings
      instead.
    - Callgrind
    - Cachegrind
7. Backwards Specification Compability
    - hybi-06
    - hybi-05
    - hybi-04
    - hixie-76
    - hixie-75

# Contributors

Here is a list of the contributors of v2.0.0 and above of the WSServer.

[Morten Houmøller Nygaard](https://github.com/mortzdk/)
[Nicolas Mora](https://github.com/babelouest/)

### Libraries 

WSServer makes use of other Open-Source libraries and code snippets. The links
listed below have all been used in some way.

* [Fast Validation of UTF-8](https://github.com/lemire/fastvalidate-utf-8/)
* [UTF-8 Decoder](http://bjoern.hoehrmann.de/utf-8/decoder/dfa/)
* [SHA1](https://tools.ietf.org/html/rfc3174)
* [Ringbuf](https://github.com/rmind/ringbuf)
* [log.c](https://github.com/rxi/log.c)
* [b64.c](https://github.com/littlstar/b64.c)
* [rpmalloc](https://github.com/mjansson/rpmalloc)
* [Threadpool](https://github.com/mbrossard/threadpool)
* [json-parser](https://github.com/udp/json-parser)
* [uthash](https://troydhanson.github.io/uthash/)
* [Http Status Codes](https://github.com/j-ulrich/http-status-codes-cpp)

# License

WSServer is licenced under the [MIT license](https://github.com/mortzdk/Websocket/blob/master/LICENSE).
