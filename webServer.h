#ifndef HEADER_H
#define HEADER_H 

#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <sstream> // for istrngstream stuff

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "logging.h"

#include <strings.h> // for bzero
#include <errno.h> // for errno
#include <sys/stat.h>
#include <cstdint> // idk what this was for VSC imported it automatically lol.

#define GET 1
#define HEAD 2
#define POST 3

constexpr std::string_view LINE_TERMINATOR = "\r\n";
// shift start of line to skip over the terminator.
//constexpr std::size_t termLen = LINE_TERMINATOR.size();  // -> 2 for "\r\n" (doesnt bloody work)
constexpr std::size_t termLen = 2; // im just gonna be lazy and manually define it.

//inline int BUFFER_SIZE = 10;

#endif
