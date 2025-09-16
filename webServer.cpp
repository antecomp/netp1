#include "webServer.h"

#define DEFAULT_PORT 1993
#define CHUNK_SIZE 10

/* 
1. Set the default return code to 400
2. Read everything up to and including the end of the header.
3. Look at the first line of the header to see if it contains a valid GET
    a. If there is a valid GET, find the filename.
    b. If there is a filename, make sure it is a valid filename according to the specs of the assignment.
        i. If the filename is valid set the return code to 200.
        ii. If the filename is invalid set the return code to 404.
*/
int readRequest(int connfd, std::string &filename) {
    int rtnCode = 400;

    std::vector<std::string> lines;

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
                    return rtnCode;
                }
                if (bytesRead < 0) {
                    if (errno == EINTR) continue;
                    ERROR << "read() failed: " << strerror(errno) << ENDL;
                    return rtnCode;
                }

                line.append(chunk, bytesRead);
                if (size_t newlinePos = line.find(LINE_TERMINATOR); newlinePos != std::string::npos) {
                    leftovers.assign(line.begin() + newlinePos + termLen, line.end());
                    line.erase(newlinePos + termLen);
                    break; // (end read)
                }
            }
        }

        // Condition for break: blank line (\r\n\r\n, but we've sanitized/clipped above so there should just be "" left)
        if (line.empty() || line == std::string(LINE_TERMINATOR)) break;
        // Otherwise just keep appending to lines.
        lines.push_back(line);
    }

    // Read lines to parse out GET request next...
    // Get should always be first, so we can just look at [0]. GET in other places is as good as invalid.
    if (!lines.empty()) {
        std::istringstream iss(lines[0]);
        std::string method, path, version;
        iss >> method;
        iss >> path;
        iss >> version;
        if(
            !iss.fail() 
            && method == "GET" 
            && version.compare(0, 5, "HTTP/") == 0
        ) {
            filename = path;
            rtnCode = 200;
            INFO << "Recieved GET request for " << filename << ENDL;
        } else {
            WARNING << "Recieved potentially malformed HTTP request" << ENDL;
            // implicitely returning 400;
        }
    }

    // Feels kinda silly that we basically don't touch the remainder of the header,
    // but we do have it and it is properly reading it!!!

    return rtnCode;

}

void processConnection(int connfd) {
    std::string filename;
    readRequest(connfd, filename);
    // based on result of readReq well move onto sending specific stuff...
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