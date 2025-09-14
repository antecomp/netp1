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

#define PORT 1993
#define BUFFER_SIZE 1024


// I think the read/write is supposed to be in a loop to hold open a connection
// void processConnection(int connfd) {
//     int bytesRead;
//     char buffer[BUFFER_SIZE]; // char[] often used to represent an arbitrary bytestream.
//     bzero(buffer, BUFFER_SIZE);

//     /* Note: Reading zero bytes means you reached the end of the file.
//     But a network never ends, so a zero means the connection failed. */
//     if((bytesRead = read(connfd, buffer, BUFFER_SIZE)) < 1) {
//         if(bytesRead < 0) {
//             FATAL << "error in processConnection/read - bytesRead reports error: " << strerror(errno) << ENDL;
//             exit(-1);
//         }

//         TRACE << "No bytes read, connection closed by client?" << ENDL;
//     }

//     INFO << "We read " << bytesRead << "bytes" << ENDL;

//     // Echo back - since we didn't exit we should have something in buffer now.
//     write(connfd, buffer, bytesRead);

// }


void processConnection(int connfd) {
    ssize_t bytesRead;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    while((bytesRead = read(connfd, buffer, BUFFER_SIZE)) > 0) {
        // Now echo back
        ssize_t bytesWrittenBack = 0;
        while(bytesWrittenBack < bytesRead) {
            ssize_t m = write(connfd, buffer + bytesWrittenBack, bytesRead - bytesWrittenBack);
            if (m > 0) {
                bytesWrittenBack += m;
            }
            if (m < 0) {
                FATAL << "error in processConnection/write - write reports error: " << strerror(errno) << ENDL;
                exit(-1);
            }
        }
    }

    if (bytesRead < 0) { // error.
        FATAL << "error in processConnection/read - read reports error: " << strerror(errno) << ENDL;
        exit(-1);
    }

    // else client closed (wrote nothing)
    INFO << "Client Closed? (read empty)" << ENDL;

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
    servaddr.sin_port = htons(PORT);

    // Bind it
    // the goofy (sockaddr*) &servaddr is a type cast to something broad, part of that weird psuedo-polymorphism thing.
    // "Notice that bind(…) uses the generic sockaddr type, but we filled in an Internet style address (sockaddr_in)."
    if(bind(listenFd, (sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
        FATAL << "bind() failed: " << strerror(errno) << ENDL;
        exit(-1);
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