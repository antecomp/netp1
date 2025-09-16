#include "webServer.h"

#define DEFAULT_PORT 1993
#define CHUNK_SIZE 10

void processConnection(int connfd) {
    std::string leftovers; // overflow from last line, will be attached to next line.

    while (1) {
        std::string line = std::move(leftovers);

        // If leftover bytes already contain a full line, peel it off now.
        if (size_t newlinePos = line.find(LINE_TERMINATOR); newlinePos != std::string::npos) {
            leftovers.assign(line.begin() + newlinePos + termLen, line.end());
            line.erase(newlinePos + 2);
        } else { // Otherwise keep reading in 10-byte chunks until we build a full line.
            char chunk[CHUNK_SIZE];
            while (true) {
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
                    line.erase(newlinePos + 2);
                    break; // (end read)
                }
            }
        }

        // REPLACE ME WITH THE STUFF TO ACTUALLY PARSE AND RESPONSE TO HTTP REQUESTS!!
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