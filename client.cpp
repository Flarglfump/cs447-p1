/*
 * Gavin Witsken
 * CS 447
 * Project 1, client.cpp
 * 03/13/2023
*/
#include "includes.hpp"

void* receiver_func(void* socket);
void* sender_func(void* socket);

//From config file
std::string SERVER_IP;
std::string SERVER_PORT;

pthread_t receiver_thread, sender_thread;

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

    pthread_create(&sender_thread, NULL, sender_func, (void*) &client_sockfd);
    pthread_create(&receiver_thread, NULL, receiver_func, (void*) &client_sockfd);

    pthread_join(receiver_thread, NULL);
    pthread_join(sender_thread, NULL);
    return 0;
}

void* receiver_func(void* socket) {
    int sockfd = * (int*) socket;

    while (1) {
        char rcv_msg_buff[MSGBUFSIZE] = {0};

        if (recv(sockfd, rcv_msg_buff, sizeof(rcv_msg_buff), 0) <= 0) {
            std::cerr << "Error: unable to receive message from server" << std::endl;
        } else {
            if (strlen(rcv_msg_buff) <= 0) {
                printf("Received empty message from server. Exiting...\n");
                break;
            } else {
                printf("Message from server: \"%s\"\n", rcv_msg_buff);
            }
        }
    }


    pthread_cancel(sender_thread);
    return NULL;
}
void* sender_func(void* socket) {
    int sockfd = * (int*) socket;

    while (1) {
        std::string msg;

        std::getline(std::cin, msg);

        if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
            std::cerr << "Unable to send message to \"" << SERVER_IP << "\"" << std::endl;
        }
    }

    return NULL;
}