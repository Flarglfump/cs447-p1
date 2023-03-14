/*
 * Gavin Witsken
 * CS 447
 * Project 1, server.cpp
 * 03/13/2023
*/
#include "includes.hpp"


//From config file
std::string NICK;
std::string PASS;
std::string PORT;
std::string SERVERS;
std::string SOCK_ADDR;

int main(int argc, char* argv[]) {
    //Validate usage
    std::string server_conf_filename;
    if (argc > 2) {
        std::cerr << "Error: Usage:\tserver <server.conf>" << std::endl;
        exit(EXIT_ARGCOUNT);
    } else if (argc == 2) {
        server_conf_filename = argv[1];
    } else {
        server_conf_filename = "server.conf";
    }

    //Configure server
    struct sockaddr_in server_address;

    if (configure_server(server_conf_filename, NICK, PASS, PORT, SERVERS, SOCK_ADDR)) {
        std::cerr << "Error: File \"" << server_conf_filename << "\" could not be read or is not formatted properly" << std::endl;
        exit(EXIT_INVALIDFILEREAD);
    }

    server_addr_init(stoi(PORT), &server_address);

    int server_sockfd = make_stream_socket();
    if (server_sockfd == -1) {
        std::cerr << "Error: Could not create socket" << std::endl;
        exit(EXIT_SOCKET);
    }

    return 0;
}