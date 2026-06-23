#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

bool read_full(int fd, char* buf, int n) {
    int total = 0;
    while (total < n) {
        int rv = read(fd, buf + total, n - total);
        if (rv <= 0) return false;
        total += rv;
    }
    return true;
}

bool write_all(int fd, const char* buf, int n) {
    int total = 0;
    while (total < n) {
        int rv = write(fd, buf + total, n - total);
        if (rv <= 0) return false;
        total += rv;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./client COMMAND [args...]\n";
        return 1;
    }

    // Combine terminal arguments into one command string
    std::string command = argv[1];
    for (int i = 2; i < argc; ++i) {
        command += " ";
        command += argv[i];
    }

    // Connect to the server
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    // 1. Send the length, then the command
    uint32_t len = command.length();
    write_all(fd, (char*)&len, 4);
    write_all(fd, command.c_str(), len);

    // 2. Read the reply length, then the reply
    uint32_t reply_len = 0;
    if (read_full(fd, (char*)&reply_len, 4)) {
        char buffer[4096] = {0};
        read_full(fd, buffer, reply_len);
        std::cout << buffer;
    }

    close(fd);
    return 0;
}