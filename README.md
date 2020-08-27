# WSServer a C WebSocket Server

WSServer is a fast, configurable, and extendable WebSocket Server for UNIX
systems written in C (C11).

As of version 2.0.0 the WSServer has been completely rewritten with many new
features, better support, better extendability and generally as a more stable
WebSocket server.

Current Version: **v1.0.0**

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

# How to get started

To build the WSServer for your UNIX system, simply run: `make release`. This
will compile extensions, subprotocols, and the binary that will be available
from: `./bin/WSServer`.

Currently the WSServer support only one extension namely the `permessage-deflate`
extension. Read more about this implementation [here](#Permessage-Deflate).

Furthermore it supports two subprotocols: [echo](#Echo) and [broadcast](#Broadcast). 
The [echo](#Echo) subprotocol is a simple protocol that sends whatever message
received, back to the same session. This is also the default protocol chosen,
if no subprotocol is provided by the session. The [broadcast](#Broadcast) subprotocol
is slightly more advanced. It sends a message from one session to all other
connected sessions. The behaviour is basically as a public chat room.

The server can be configured by providing a `-c [path_to_config_file.json]`
flag. If no configuration is provided, the server will run with a default
configuration, that support WebSocket over HTTP on port 80. You can read more
about the structure and description of the configuration file in the
[Configuration](#Configuration) section.

### Log

WSServer keeps a log file at `logs/WSServer.log`. The detail of the log is
defined by the `log_level` value in the configuration. Default for a release
build is 3 (FATAL, ERROR, WARN, INFO). Default for a test build is 5 (FATAL,
ERROR, WARN, INFO, DEBUG, TRACE).

The log file is a good tool for discovering misbehavior of the server, such as
when the server isn't able to start, since port 80 is already occupied by
anotherrserver instance.

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

If you want to run the Autobahn testsuite and the unit tests yourself, further
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
pacman -S docker
pacman -S criterion
```

##### MacOS
```
brew install docker
brew install snaipe/soft/criterion
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

##### Size

TODO

##### Pool

TODO

##### SSL (WSS)

TODO

OpenSSL

# Extensions {#Extensions}
TODO

### Permessage-deflate {#Permessage-Deflate}
TODO

# Subprotocols {#Subprotocols}
TODO

### Echo {#Echo}
TODO

### Broadcast {#Broadcast}
TODO

### Client Authentication

Notes: 
Send query parameters using the path from the websocket connecting uri.
The path and cookies are send through to the subprotocol in the `onConnect` call.

# Documentation

WSServer does not have anykg
TODO

# Testing

TODO

### Unit tests

TODO

### Autobahn Testsuite

TODO

### Code coverage

TODO

# Contributors

[Morten Houmøller Nygaard](https://github.com/mortzdk/)

### Libraries 

WSServer makes use of other Open-Source libraries and code snippets. The links
listed below have all been used in some way.

*[Fast Validation of UTF-8](https://github.com/lemire/fastvalidate-utf-8/)
*[UTF-8 Decoder](http://bjoern.hoehrmann.de/utf-8/decoder/dfa/)
*[SHA1](https://tools.ietf.org/html/rfc3174)
*[Ringbuf](https://github.com/rmind/ringbuf)
*[Log.c](https://github.com/rxi/log.c)
*[B64.c](https://github.com/littlstar/b64.c)
*[rpmalloc](https://github.com/mjansson/rpmalloc)
*[Threadpool](https://github.com/mbrossard/threadpool)
*[json-parser](https://github.com/udp/json-parser)

# License

WSServer is licenced under the [MIT license](https://github.com/mortzdk/Websocket/blob/master/LICENSE).
