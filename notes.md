## Address Structure (sockaddr_in)
Used to configure our socket.
```cpp
struct sockaddr_in
{
    sa_family_t sa_prefix##family;  /* Address family. (ipv4 = AF_INET) */
    in_port_t sin_port;            /* Port number. */
    struct in_addr sin_addr;       /* Internet (IP) address. */

    /* Pad to size of `struct sockaddr'. */
    unsigned char sin_zero[sizeof (struct sockaddr) - …];
};
```

When working with networking codes etc, make sure to used fixed width/endianness explicitely for your type declarations (as different OS's do different things by default, so transmitted data may get slopped). More specifically; `u_int8_t`, `u_int16_t` & `u_int32_t`.

There are also functions to help convert between host and network standard;
* `htons()` - Host to network short (int16)
* `htonl` - Host to network long (int32)
* `ntohs()` - Network to host short
* `ntohl()` - Network to host long

Regarding endianess, **network standard is big-endian**. The above methods should handle the endianness for you. Rule of thumb is to apply these at any demarc point between you and the network.

The strange struct use above is because C (original language of implementation here) has no polymorphism, so it's faked with structs and goofy data types.

## After configuring you can bind;
The goofy (sockaddr*) &servaddr is a type cast to something broad, part of that weird psuedo-polymorphism thing.
 > "Notice that bind(…) uses the generic sockaddr type, but we filled in an Internet style address (sockaddr_in)."

`int bind( int listenSocketFd, const struct sockaddr *my_addr,
socklen_t addrlen);`

### Common bind() errors
* EACCES Don't have access to the fd
* EADDRINUSE Local address in use.
* EAFNOSUPPORT Address family not supported.
* EFAULT Address of my_addr is invalid.
* EBADF Bad file descriptor.
* ENOTSOCK The fd points to a file not a socket.

## Now we create and link the listening queue
backlog indicates how many people we're willing to queue/serve at a time, clients connecting beyond this number are dropped (drop behavior varies by OS - silent or ECONNREFUSED f.e)

`int listen(int socktd, int backlog)`
### Common listen errors;
* EBADF Bad file descriptor.
* EADDRINUSE Local address in use.
* EAFNOSUPPORT Address family not supported.
* ENOTSOCK The argument does not point to a socket.
* EOPNOTSUPP The socket type does not support listen().

## then we listen...
`int accept( int sockfd, struct sockaddr *addr,socklen_t *addrlen);`

**Common errors**
* EAGAIN Non-blocking socket would block
* EBADF File descriptor is bad.
* EINTR System call was interrupted.
* EMFILE File limit reached.
* ENOTSOCK FD points to a file not a socket
* EOPNOTSUPP The referenced socket is not of type SOCK_STREAM
> "Notice that accept()blocks until a connection comes in. When it does return we have two sockets. The original (listening socket) AND the new connected socket."

Accept gives us a new file descriptor, that we can then hand off to start processing the transaction. Calling accept again will move on to the next connection. For the actual implementation we just wrap this in a big loop of accept -> process -> (repeat)
```cpp
while (1) {
    int connfd = -1;
    if ((connfd = accept(listenfd, (sockaddr *) NULL, NULL)) < 0) {
        cout << "accept() failed: " << strerror(errno) << endl;
        exit(-1);
    }
    processConnection(connfd);
}
```

## Read & Write.
(TODO NEXT: Slide 51)

---

## logging.h 
Shorthand way of adding logging messages (aliases of cout/cerr), 
Aliases correspond to increasing log levels;
1. FATAL
2. ERROR
3. WARNING
4. INFO
5. DEBUG
6. TRACE 

example usage;
```c
DEBUG << "This is a debug message" << ENDL;
```