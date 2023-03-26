/*
 * Gavin Witsken
 * CS 447
 * Project 1, server.cpp
 * 03/13/2023
*/
#include "includes.hpp"

User_List users;

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
    std::string client_hostname = "";

    char hostnamebuff[100];
    char servnamebuff[100];
    if (getnameinfo((struct sockaddr*) &client_address, sizeof(client_address), hostnamebuff, 100, servnamebuff, 100, 0)) {
        std::cerr << "Unable to resolve hostname and service name of client " << client_ip_addr << std::endl;
    } else {
        client_hostname = hostnamebuff;
    }

    //Add client to User list
    users.add_user(client_ip_addr);

    
    //Wait for client messages and handle on loop

    while (1) {
        char received_msg_buff[MSGBUFSIZE];
        std::string received_msg;
        bool do_message = true;
        bool do_quit = false;

        if (recv(sockfd, received_msg_buff, sizeof(received_msg_buff), 0) <= 0) {
            std::cerr << "Unable to receive message from \"" << client_ip_addr << "\" or client closed connection" << std::endl;
        } else {
            received_msg = received_msg_buff;
            std::cout << "Message from client (" << client_ip_addr << "): \"" << received_msg << "\"\n" ;
        }

        //Client was terminated, so terminate connection
        if (received_msg == "") {
            break;
        }

        //Tokenize message via spaces
        std::vector<std::string> args = tokenize(received_msg);

        std::string cmd = args[0];

        int cmd_key = get_command_key(cmd);

        std::string server_name = NICK;
        std::string reply_code = "";
        std::string reply_details = "";
        std::stringstream reply_message;

        switch(cmd_key) {
            case CMD_NICK:
            if (args.size() == 1) {
                reply_code = ERR_NONICKNAMEGIVEN;
                reply_details = ":No nickname given" ;
            } else if (args.size() > 2) {
                reply_code = ERR_ERRONEUSNICKNAME;
                for (int i = 1; i < args.size(); ++i) {
                    reply_details += " " + args[i];
                }
                reply_details += ":Erroneous nickname";
            } else {
                std::string nickname = args[1];
                std::cerr << "Nickname: " << nickname << std::endl;

                //check if valid format
                if (!is_valid_nickname(nickname)) {
                    reply_code = ERR_ERRONEUSNICKNAME;
                    reply_details = nickname + " :Erroneous nickname";
                }
                //check if already in use
                else if (users.nick_exists(nickname)) {
                    reply_code = ERR_NICKNAMEINUSE;
                    reply_details = nickname + " :Nickname is already in use";
                } 
                //nickname can be set
                else {
                    users.update_nick(client_ip_addr, nickname); 

                    int user_idx = users.get_user_idx_via_nick(nickname);
                    std::cerr << "User idx: " << user_idx << std::endl;
                    User user_info = users.users[user_idx];
                    if (user_info.username == "") {
                        do_message = false;
                        // reply_code = RPL_WELCOME;
                        // reply_details = nickname + ": Welcome to the Internet Relay Network " + nickname + "!" + user_info.username + "@" + client_hostname;
                    } else {
                        reply_code = RPL_WELCOME;
                        reply_details = nickname + ": Welcome to the Internet Relay Network " + nickname + "!" + user_info.username + "@" + client_hostname;
                    }
                }
            }
            break;

            case CMD_USER:

            break;

            case CMD_QUIT:

            break;

            case CMD_JOIN:

            break;

            case CMD_PART:

            break;

            case CMD_TOPIC:

            break;

            case CMD_TIME:

            break;

            case CMD_NAMES:

            break;

            case CMD_PRIVMSG:

            break;

            case CMD_INVALID:
            default:


            break;
        }

        if (do_quit) {
            break;
        }

        std::cerr << "Made it through switch statement" << std::endl;
        
        if (do_message) {
            //Put message components together
            reply_message << ":" << server_name << " " << reply_code << " " << reply_details;
            std::string reply_message_str = reply_message.str();
            
            std::cerr << "Constructed reply message: \"" << reply_message_str << "\"" << std::endl;

            //Send message - if failed, terminate the connection and thread
            if (send(sockfd, reply_message_str.c_str(), reply_message_str.size() + 1, 0) <= 0) {
                std::cerr << "Error: unable to send reply message" << std::endl;
                break;
            } else {
                std::cout << "Sent message to client (" << client_ip_addr << "): \"" << reply_message_str << "\"" << std::endl;
            }
        }
    }

    //Critical Section start
    int user_idx = users.get_user_idx_via_ip(client_ip_addr);
    if (user_idx != -1) {
        users.users.erase(users.users.begin() + user_idx);
    }
    //Critical Section end

    close(sockfd);
    return NULL;
}