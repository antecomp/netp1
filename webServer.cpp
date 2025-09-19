#include "webServer.h"
#include "logging.h"
#include <fcntl.h>

std::filesystem::path webRoot = std::filesystem::current_path() / "data";

bool check_for_file(const std::string &reqPath, std::string &resolvedPath) {
    if(
        reqPath.empty() ||
        reqPath.front() != '/' ||
        reqPath.find("..") != std::string::npos
    ) {
        return false;
    }

    std::string localPath = reqPath.substr(1); // drop leading slash

    std::filesystem::path fullPath = webRoot / localPath;
    if(
        !std::filesystem::exists(fullPath) || 
        !std::filesystem::is_regular_file(fullPath)
    ) {
        return false;
    }

    resolvedPath = fullPath.string();
    return true;
}

bool is_file_valid(const std::string &filename) {
    DEBUG << "checking file validity for filename" << filename << ENDL;
    std::filesystem::path p(filename);
    std::string base = p.filename().string();
    static const std::regex allowed(R"(^[A-Za-z]+[0-9]+\.(html|jpg)$)", std::regex::icase);
    return std::regex_match(base, allowed);
}

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
        std::string method, reqPath, version;
        iss >> method;
        iss >> reqPath;
        iss >> version;
        if(
            !iss.fail() 
            && method == "GET" 
            && version.compare(0, 5, "HTTP/") == 0
        ) {
            // this also sets filename to be the proper local path (string)
            // filename should update during this short-circuit (check_for_file modifies it)
            if(check_for_file(reqPath, filename) && is_file_valid(filename)) {
                rtnCode = 200;
            } else {
                rtnCode = 404;
            }
            INFO << "Recieved GET request for " << reqPath << " Providing status: " << rtnCode << ENDL;
        } else {
            INFO << "Recieved potentially malformed HTTP request" << ENDL;
            // implicitely returning 400;
        }
    }

    // Feels kinda silly that we basically don't touch the remainder of the header,
    // but we do have it and it is properly reading it!!!

    return rtnCode;
}

/*
sendLine(socketFD, std::string &stringToSend)
    1. Convert the std::string to an array that is 2 bytes longer than the string.
    2. Replace the last two bytes of the array with the <CR> and <LF>
    3. Use write to send that array.
*/
void sendLine(int connfd, const std::string &stringToSend) {
    std::string line = stringToSend + std::string(LINE_TERMINATOR);

    std::size_t sent = 0;
    while(sent < line.size()) {
        /* replaced write with send, to include MSG_NOSIGNAL
           this prevents SIGPIPE (client closed during write) from terminating the process.
           send, like write, should still just work with raw bytes.
        */
        ssize_t written = send(connfd, line.data() + sent, line.size() - sent, MSG_NOSIGNAL);
        if(written < 0) {
            if (errno == EINTR) continue;
            if (errno == EPIPE) {
                WARNING << "write() failed: connection closed mid-write." << ENDL;
                return;
            }
            ERROR << "write() failed in sendLine: " << strerror(errno) << ENDL;
            return;
        }
        sent += static_cast<std::size_t>(written); // (convert written to unsigned to append nicely)
    }
}

void send404(int connfd) {
    sendLine(connfd, "HTTP/1.1 404 Not Found ");
    sendLine(connfd, "Content-Type: text/html; charset=UTF-8");
    sendLine(connfd, "");
    sendLine(connfd, "<!DOCTYPE html>");
    sendLine(connfd, "<html lang=\"en\"><head><meta charset=\"utf-8\"><title>404</title></head>");
    sendLine(connfd, "<body><h1>404 :(</h1><p>The requested file was not found.</p></body></html>");
}

void send400(int connfd) {
    sendLine(connfd, "HTTP/1.1 400 Bad Request");
    sendLine(connfd, "");
}

/*
// **************************************************************************
// * Send a 200
// **************************************************************************
sendFile(socketFD, filename)
1. Use the stat() function call to find the size of the file.
2. If stat fails you don’t have read permission or the file does not exist.
    a. Send a 404 by calling send404()
    b. exit the send200() function.
3. Using the sendLine() function you wrote send the header:
4. Send a properly formatted HTTP response with the code 200
5. Send the content type depending on the type of file (text/html or image/jpeg)
6. Send the content-length
    a. Note – if the content length and/or file type are not sent correctly, your browser will not display the file correctly.
7. Send the file itself.
    a. Open the file.
    b. Allocate 10 bytes of memory with malloc() or new[]
    c. While #of-bytes-sent != size-of-file
        i. Clear out the memory with bzero or something similar.
        ii. read() up to 10 bytes from the file into your memory buffer
        iii. write() the number of bytes you read
8. when you are done you can just return. Since you set the content- length you don’t send the line terminator at the end of the file.
*/
void sendFile(int connfd, std::string filename) {
    struct stat st; 
    if(stat(filename.c_str(), &st) < 0) {
        // don’t have read permission or the file does not exist.
        DEBUG << "cannot send file (failed at size check), likely do not have permissions. Falling back to 404." << ENDL;
        send404(connfd);
        return;
    }

    TRACE << "sending file of size " << st.st_size << ENDL;

    auto filesize = static_cast<uint64_t>(st.st_size); // make format proper for Content-Length header.

    std::filesystem::path p(filename);
    std::string ext = p.extension().string();
    TRACE << "requested file has extension " << ext << ENDL;
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

    std::string contentType = "application/octet-stream"; // fallback (should never see this).
    if (ext == ".html" || ext == ".htm")  contentType = "text/html; charset=utf-8";
    else if (ext == ".jpg" || ext == ".jpeg") contentType = "image/jpeg";


    int filefd = open(filename.c_str(), O_RDONLY);
    if (filefd < 0) {
        ERROR << "open() failed: " << strerror(errno) << ENDL;
        send404(connfd);
        return;
    }

    sendLine(connfd, "HTTP/1.1 200 OK");
    sendLine(connfd, "Content-Type: " + contentType); // determine type of file first!
    sendLine(connfd, "Content-Length: " + std::to_string(filesize));
    sendLine(connfd, "");
    //sendLine(connfd, "Bogus Content To Test!");

    //send file bytes
    char *buffer = new char[CHUNK_SIZE];
    uint64_t totalSent = 0;

    while(totalSent < filesize) {
        bzero(buffer, CHUNK_SIZE);

        ssize_t chunkRead = read(filefd, buffer, CHUNK_SIZE);
        if(chunkRead < 0) {
            if(errno == EINTR) continue;
            ERROR << "read() failed while sending file: " << strerror(errno) << ENDL;
            delete[] buffer;
            close(filefd);
            return;
        }
        if(chunkRead == 0) {
            WARNING << "Unexpected EOF while sending file" << ENDL;
            break;
        }

        ssize_t chunkWritten = 0;
        while(chunkWritten < chunkRead) {
            ssize_t written = send(connfd, buffer + chunkWritten, chunkRead - chunkWritten, MSG_NOSIGNAL);
            if (written < 0) {
                if (errno == EINTR) continue;
                if(errno == EPIPE) {
                    WARNING << "Client closed connection while sending file." << ENDL;
                }  else {
                    ERROR << "send() faild while sending file: " << strerror(errno) << ENDL;
                }
                delete[] buffer;
                close(filefd);
                return;
            }
            chunkWritten += written;
        }

        totalSent += static_cast<uint64_t>(chunkRead);
    }

    delete[] buffer;
    close(filefd); // need to do this to prevent leak :^)

}

void processConnection(int connfd) {
    std::string filename;
    int rtnCode = readRequest(connfd, filename);
    //auto codeString = std::to_string(rtnCode);
    //sendLine(connfd, codeString); // test response. (works :))

    // different responses...
    switch(rtnCode) {
        case 404:
            send404(connfd);
            break;
        case 400:
            send400(connfd);
            break;
        case 200:
            sendFile(connfd, filename);
            break;
        default:
            WARNING << "[processConnection] Somehow we got an unhandled rtnCode: " << rtnCode << ENDL;
            send400(connfd);
    }
}


int main(int argc, char *argv[]) {

    // Process cl args (taken from template)
    int opt = 0;
    while ((opt = getopt(argc, argv, "d:")) != -1) {

        switch (opt) {
        case 'd':
            LOG_LEVEL = std::stoi(optarg);
            break;
        case ':':
        case '?':
        default:
            std::cout << "useage: " << argv[0] << " -d LOG_LEVEL" << std::endl;
            exit(-1);
        }
    }

    // Create the socket - it makes an oldschool typeless file descriptor thingy
    DEBUG << "init: calling socket()" << ENDL;
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
    DEBUG << "init: attempting to bind socket." << ENDL;

    int port = DEFAULT_PORT;
    while(1) {
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

        //INFO << "bound to port " << port << ENDL;
        // Instead: going to always print bound port regardless of logging mode...
        std::cout << "bound to port " << port << std::endl;

        break;
    }

    DEBUG << "init: Configuring listen() " << ENDL;

    // Create the listening queue and link it with socket.
    int queuedepth = 1;
    if (listen(listenFd, queuedepth) < 0) {
        FATAL << "listen() failed: " << strerror(errno) << ENDL;
        exit(-1);
    }

    // Wait for connection w/ accept call. Da bigol' server loop

    DEBUG << "init: now entering main loop (wait and accept() cycle)" << ENDL;

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