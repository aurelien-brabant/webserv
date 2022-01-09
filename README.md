A tiny but highly configurable HTTP/1.1 server implementation, made for learning purpose.

# Disclaimer

This web server has been developed for learning purpose, and is not, by ANY mean, intended for real world usage.

It doesn't even completely conform to the HTTP/1.1 specification, and there is room for a lot of improvements.

# Features

- Respond to any idiomatic HTTP request that has at least the `Host` header field
- Configurable routes
- Configurable request timeout
- Configurable custom error pages
- Configurable upload mechanism: only handles `multipart/form-data` content type
- Support for the `GET`, `POST` and `DELETE` HTTP methods.
- HTTP redirections
- File deletion mechanism (through the use of the `DELETE` HTTP verb) **in upload directories** only
- Common Gateway Interface implementation: designed to support `php-cgi`, may actually support a lot of others
- Careful error handling (catches ill-formed requests)
- HTTP keep-alive mechanism to avoid excessive TCP SYN requests (connections made by the browser are reused as much as possible)
- Support for chunked HTTP requests
- Configurable body max. size / upload max. size
- I/O multiplexing: the server handles concurrent requests (although it's mono-threaded)
- Match different configuration blocks depending on the `Host` header field

# Configuration format

For more information about our configuration format and our API, feel free to check the [wiki](https://github.com/busshi/webserv/wiki). While some parts are
outdated or incomplete, it still is very relevant and worth reading.

# Build

## Prerequisites

- A C++ compiler (tested with clang++ and g++)
- A UNIX-based operating system: the web server has been mainly built to work on Linux but should also work on MacOS.
- The `make` build tool

## Compile the server

A simple `make` command should make the `webserv` executable. This executable is the server itself and can then
be started by providing it a configuration file like so:

```sh
./webserv ./asset/config/simple.conf
```

## Enable server errors insights

By default, when a HTTP error is leveraged by the server, simple HTML that indicates the error status and a short description of the status is sent to the user agent.
For security sake, no specific information about why the error happened is delivered to the client.

However, by passing to webserv the `ERROR_HINT` environment variable, the response will contain some insights about why the error actually happened.
This may be useful for quick and easy troubleshooting (although error logs are generally a much better option).

Starting the server as showed below should do the trick.

```sh
ERROR_HINT=1 ./webserv my_config
```
