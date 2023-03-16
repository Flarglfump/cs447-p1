/*
 * Gavin Witsken
 * CS 447
 * Project 1, server.cpp
 * 03/13/2023
*/
#include "includes.hpp"

//Globals - accessible in any thread
#define BACKLOG 10

//From config file
std::string NICK;
std::string PASS;
std::string PORT;
std::string SERVERS;
std::string SOCK_ADDR;

void* handle_connection(void* new_client_info);

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

    //Configure server & set up socket
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
    if (bind(server_sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) == -1) {
        std::cerr << "Error: Could not bind server socket to specified port" << std::endl;
        exit(EXIT_BIND);
    }
    if (listen(server_sockfd, BACKLOG) == -1) {
        std::cerr << "Error: Could not start listening for incoming connections" << std::endl;
        exit(EXIT_LISTEN);
    }

    //Wait for connections
    int new_client_sockfd = -1;
    struct sockaddr_in new_client_addr;
    socklen_t new_client_addr_size= sizeof(new_client_addr);

    Client_Info* new_client_info;

    std::cout << "Server: ready to receive incoming connections..." << std::endl;
    while (1) {
        new_client_sockfd = accept(server_sockfd, (struct sockaddr*) &new_client_addr, &new_client_addr_size);
        if (new_client_sockfd == -1) {
            std::cerr << "Error: unable to accept new connection" << std::endl;
            continue;
        } 

        //Connection successful, send to new thread to handle after packing struct with info
        new_client_info = (Client_Info*) calloc(1, sizeof(Client_Info));
        new_client_info->addr = new_client_addr;
        new_client_info->sockfd = new_client_sockfd;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_connection, (void*) new_client_info);
        
        new_client_info = NULL;
        new_client_sockfd = -1;
        new_client_addr_size= sizeof(new_client_addr);
    }

    std::cerr << "Server process terminating..." << std::endl;

    return 0;
}

void* handle_connection(void* new_client_info) {
    //Store client data locally and free previously used memory
    int sockfd = ((Client_Info*) new_client_info)->sockfd;
    struct sockaddr_in client_address = ((Client_Info*) new_client_info)->addr;
    free(new_client_info);

    char client_addr_str_buff[100] = {0};
    std::string client_ip_addr = std::string(inet_ntop(client_address.sin_family, (void*) &client_address.sin_addr, client_addr_str_buff, sizeof(client_addr_str_buff)));

    std::string response_msg = "Hello, I received your message!";
    if (send(sockfd, response_msg.c_str(), response_msg.size()+1, 0) == -1) {
        std::cerr << "Unable to send message to \"" << client_ip_addr << "\"" << std::endl;
    }

    char received_msg_buff[MSGBUFSIZE];
    if (recv(sockfd, received_msg_buff, sizeof(received_msg_buff), 0) == -1) {
        std::cerr << "Unable to receive message from \"" << client_ip_addr << "\"" << std::endl;
    } else {
        printf("Message from client: \"%s\"\n", received_msg_buff);
    }


    //Client needs to register

    return NULL;
}