#include "webServer.h"

/** 
1. Create the socket
    • At this point it is just a data structure that is not doing anything.
2. Tell the socket what address and port to use.
    • Called binding the socket
3. Tell our socket how big the backlog of connections it should have (called the listening queue)
    • Our socket is now ready to accept connections.
4. Accept and process a connection request.
    • If there are no requests in the queue the server will wait for one.
    • The client makes the connection request, and the OS takes care of setting everything up.
    • The original socket is still listening and putting requests in the queue.
5. Once the connection exists the client can send data to the server with the write function.
6. The server will read the data and and act on the message.
7. The server can write data back to the client.
    • The connection can be used for as long as you like, with both the client and the server reading and writing.
8. When we done either the client or the server can close the connection.
    • For the assignment, your server will close. 
*/

#define DEFAULT_PORT 1993
//#define BUFFER_SIZE 1024
#define CHUNK_SIZE 10

// First, a very generic version (to see if this works at all.) 
// void processConnection(int connfd) {
//     ssize_t bytesRead;
//     char buffer[BUFFER_SIZE];
//     bzero(buffer, BUFFER_SIZE);

//     // Read here is *also* blocking! It will wait for data to arrive before returning and re-entering this loop.
//     while((bytesRead = read(connfd, buffer, BUFFER_SIZE)) > 0) {
//         // Now echo back
//         ssize_t bytesWrittenBack = 0;
//         while(bytesWrittenBack < bytesRead) {
//             ssize_t m = write(connfd, buffer + bytesWrittenBack, bytesRead - bytesWrittenBack);
//             if (m > 0) {
//                 bytesWrittenBack += m;
//             }
//             if (m < 0) {
//                 FATAL << "error in processConnection/write - write reports error: " << strerror(errno) << ENDL;
//                 exit(-1);
//             }
//         }
//     }

//     if (bytesRead < 0) { // error.
//         FATAL << "error in processConnection/read - read reports error: " << strerror(errno) << ENDL;
//         exit(-1);
//     }

//     // else client closed (wrote nothing)
//     INFO << "Client Closed? (read empty)" << ENDL;
// }

/*  - Hint provided psuedocode.
void processConnection(int socketFD) will:
1. Loop until the word “CLOSE” is sent over the network
    a. Initialize std container (std::string, std::array etc) that will hold the whole line.
    b. Initialize a 10 byte buffer
    c. Loop until the line terminator is found.
        i. Read up to 10 bytes from the network with the read() system call.
        ii. Append the bytes received to the end of the container (maybe less than 10).
    d. Send the whole contents of the container back to the client. Don’t forget to convert it back to raw binary data.
    e. Check for the word “CLOSE”
2. return
*/

// Line based process connection (more robust).
void processConnection(int connfd) {
    std::string leftovers; // overflow from last line, will be attached to next line.

    while (1) {
        std::string line = std::move(leftovers);

        // If leftover bytes already contain a full line, peel it off now.
        if (size_t newlinePos = line.find(LINE_TERMINATOR); newlinePos != std::string::npos) {
            leftovers.assign(line.begin() + newlinePos + termLen, line.end());
            line.erase(newlinePos + termLen);
        } else { // Otherwise keep reading in 10-byte chunks until we build a full line.
            char chunk[CHUNK_SIZE];
            while (true) { // read loop (go in chunks, appending to line until we hit terminator)
                ssize_t bytesRead = read(connfd, chunk, sizeof(chunk));
                if (bytesRead == 0) {
                    INFO << "Client Closed Connection (Empty Read)" << ENDL;
                    return;
                }
                if (bytesRead < 0) {
                    if (errno == EINTR) continue;
                    ERROR << "read() failed: " << strerror(errno) << ENDL;
                    return;
                }

                line.append(chunk, bytesRead);
                if (size_t newlinePos = line.find(LINE_TERMINATOR); newlinePos != std::string::npos) {
                    leftovers.assign(line.begin() + newlinePos + termLen, line.end());
                    line.erase(newlinePos + termLen);
                    break; // (end read)
                }
            }
        }

        // Echo back. I think this is what'll be replaced with all our cool parsing HTTP :D
        const char* data = line.data(); // convert back to raw data (I think this is the right method for that)
        size_t remaining = line.size();
        while (remaining > 0) {
            ssize_t written = write(connfd, data, remaining);
            if (written < 0) {
                // same thing regarding EINTR here as w/ read
                if(errno == EINTR) continue;
                ERROR << "write() failed: " << strerror(errno) << ENDL;
                return;
            }
            data += written;
            remaining -= written;
        }

        
        // Condition to break this loop: Word "CLOSE" sent...
        // string_view is this non-mutable window thingy we can use to trim our line of esc chars.
        std::string_view trimmed = line;
        while(!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
            trimmed.remove_suffix(1);
        }
        if (trimmed == "CLOSE") {
            INFO << "Closing by will of client: CLOSE command" << ENDL;
            return;
        }

    }

}


int main() {

    // Create the socket - it makes an oldschool typeless file descriptor thingy
    int listenFd = -1;
    if ((listenFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        FATAL << "Failed to create listening socket " << strerror(errno) << ENDL;
        exit(-1);
    }

    // Configure da socket.
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr)); // Zero it out.
    servaddr.sin_family = AF_INET; // ipv4
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on everything

    // track if we got the port (for attempt looping)
    bool isPortBound = false;
    int port = DEFAULT_PORT;
    while(!isPortBound) {
        servaddr.sin_port = htons(port);

        // Bind it
        // the goofy (sockaddr*) &servaddr is a type cast to something broad, part of that weird psuedo-polymorphism thing.
        // "Notice that bind(…) uses the generic sockaddr type, but we filled in an Internet style address (sockaddr_in)."
        if(bind(listenFd, (sockaddr*) &servaddr, sizeof(servaddr)) < 0) {

            if(errno == EADDRINUSE) {
                port += 1;
                continue;
            }

            FATAL << "bind() failed: " << strerror(errno) << ENDL;
            exit(-1);
        }

        INFO << "bound to port " << port << ENDL;
        isPortBound = true;
    }

    // Create the listening queue and link it with socket.
    int queuedepth = 1;
    if (listen(listenFd, queuedepth) < 0) {
        FATAL << "listen() failed: " << strerror(errno) << ENDL;
        exit(-1);
    }

    // Wait for connection w/ accept call. Da bigol' server loop
    while(1) {
        int connfd = -1;
        // Accept blocks until we actually have a connection.
        if((connfd = accept(listenFd, (sockaddr*) NULL, NULL)) < 0) {
            FATAL << "accept() failed: " << strerror(errno) << ENDL;
            exit(-1);
        } 

        processConnection(connfd);
        close(connfd);
    }
}