/*
 * Gavin Witsken
 * CS 447
 * Project 1, client.cpp
 * 03/13/2023
*/
#include "includes.hpp"

//From config file
std::string SERVER_IP;
std::string SERVER_PORT;

int main(int argc, char* argv[]) {
    //Validate usage
    std::string client_conf_filename;
    if (argc > 2) {
        std::cerr << "Error: Usage:\tserver <server.conf>" << std::endl;
        exit(EXIT_ARGCOUNT);
    } else if (argc == 2) {
        client_conf_filename = argv[1];
    } else {
        client_conf_filename = "client.conf";
    }

    //Configure server & set up socket
    struct sockaddr_in client_address;

    if (configure_client(client_conf_filename, SERVER_IP, SERVER_PORT)) {
        std::cerr << "Error: File \"" << client_conf_filename << "\" could not be read or is not formatted properly" << std::endl;
        exit(EXIT_INVALIDFILEREAD);
    }

    //Load up server address struct
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(stoi(SERVER_PORT));
    inet_aton(SERVER_IP.c_str(), &server_address.sin_addr);

    int client_sockfd = make_stream_socket();
    if (client_sockfd == -1) {
        std::cerr << "Error: Could not create socket" << std::endl;
        exit(EXIT_SOCKET);
    }

    if (connect(client_sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) == -1) {
        perror("connect");
        std::cerr << "Error: Unable to establish connection to server" << std::endl;
        exit(EXIT_CONNECT);
    }

    char rcv_msg_buff[MSGBUFSIZE] = {0};
    if (recv(client_sockfd, rcv_msg_buff, sizeof(rcv_msg_buff), 0) == -1) {
        std::cerr << "Error: unable to receive message from server" << std::endl;
    } else {
        printf("Message from server: \"%s\"\n", rcv_msg_buff);
    }

    std::string msg = "Cool beans bro";
    if (send(client_sockfd, msg.c_str(), msg.size()+1, 0) == -1) {
        std::cerr << "Unable to send message to \"" << SERVER_IP << "\"" << std::endl;
    }

    return 0;
}