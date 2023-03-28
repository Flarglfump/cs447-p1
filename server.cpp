/*
 * Gavin Witsken
 * CS 447
 * Project 1, server.cpp
 * 03/13/2023
*/
#include "includes.hpp"

User_List users;
Channel_List channels;
std::vector<std::string> server_list;

//Globals - accessible in any thread
#define BACKLOG 10

//From config file
std::string NICK;
std::string PASS;
std::string PORT;
std::string SERVERS;
std::string SOCK_ADDR;

void* handle_connection(void* new_client_info);
void wipe_user(std::string ip_addr_str);

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
    users.add_user(client_ip_addr, sockfd);

    
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
        
        // std::cerr << "Args:\n";
        // for (int i = 0; i < args.size(); ++i) {
        //     std::cerr << i << ": " << args[i] << std::endl;
        // }

        std::string cmd = args[0];
        std::string realname = "";

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
                // std::cerr << "Nickname: " << nickname << std::endl;

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

                    std::cerr << "Nickname can be set." << std::endl;

                    int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                    std::string old_nick = users.users[user_idx].nickname;

                    if (users.update_nick(client_ip_addr, nickname) != 0) {
                        //Could not update nick - terminate connection
                        do_quit = true;
                        break;
                    } 

                    
                    std::cerr << "User idx: " << user_idx << std::endl;
                    User user_info = users.users[user_idx];
                    if (user_info.username == "") {
                        do_message = false;
                        // reply_code = RPL_WELCOME;
                        // reply_details = nickname + ": Welcome to the Internet Relay Network " + nickname + "!" + user_info.username + "@" + client_hostname;
                    } else {
                        if (users.users[user_idx].registered == false) {
                            users.users[user_idx].registered = true;
                            reply_code = RPL_WELCOME;
                            reply_details = nickname + " :Welcome to the Internet Relay Network " + nickname + "!" + user_info.username + "@" + client_hostname;
                        } else {
                            do_message = false;
                            std::string msg = old_nick + "!" + user_info.username + "@" + client_hostname + " " + cmd + nickname;
                        }
                    }
                }
            }
            break;

            case CMD_USER:
            if (args.size() < 5) {
                reply_code = ERR_NEEDMOREPARAMS;
                reply_details = cmd + " :Not enough parameters";
            } else {
                //Check if user is already registered
                int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                if (user_idx == -1) { 
                    //User does not exist - shouldn't happen
                    do_quit = true;
                    break;
                } else {
                    User user_data = users.users[user_idx];
                    if (user_data.username != "") {
                        //User is already registered
                        reply_code = ERR_ALREADYREGISTRED;
                        reply_details = ":Unauthorized command (already registered)";
                    } else {
                        //User can be registered
                        //Validate username and real name
                        realname = "";
                        for (int i = 4; i < args.size(); ++i) {   
                            if (i != 4) {
                                realname += " ";
                            }

                            if (args[4][0] == ':') {
                                realname += args[i].substr(1, args[i].size()-1);
                            } else {
                                realname += args[i];
                            }
                        }

                        if (!is_valid_username(args[1])) {
                            reply_code = ERR_ERRONEUSUSERNAME;
                            reply_details = args[1] + " :Erroneous username";
                        } else if (!is_valid_realname(realname)) {
                            reply_code = ERR_ERRONEOUSREALNAME;
                            reply_details = realname + " :Erroneous realname";
                        } else {
                            //Username and realname are valid - need to update entry
                            if (users.update_user(client_ip_addr, args[1]) != 0 || users.update_real(client_ip_addr, realname) != 0) {
                                //Could not update username or realname - terminate connection
                                do_quit = true;
                                break;
                            }
                            else {
                                //Info was updated - make sure Nick is there to send message
                                if (user_data.nickname == "") {
                                    do_message = false;
                                } else {
                                    users.users[user_idx].registered = true;
                                    reply_code = RPL_WELCOME;
                                    reply_details = user_data.nickname + " :Welcome to the Internet Relay Network " + user_data.nickname + "!" + args[1] + "@" + client_hostname;
                                }
                            }
                        }
                    }
                }
            }
            break;

            case CMD_QUIT:
            if (args.size() == 1) {
                //No message - just quit
                do_quit = true;
                int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                User usr = users.users[user_idx];
                std::string msg = ":" + usr.nickname + "!" + usr.username + "@" + client_hostname + " " + cmd;
                users.broadcast(msg);
            } else {
                //There is a message
                do_quit = true;
                int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                User usr = users.users[user_idx];
                std::string msg = ":" + usr.nickname + "!" + usr.username + "@" + client_hostname + " " + cmd + " ";

                if (args[1][0] != ':') {
                    msg += ':';
                }  
                for (int i = 1; i < args.size(); ++i) {
                    if (i != 1) {
                        msg += " ";
                    }
                    msg += args[i];
                }
                users.broadcast(msg);
            }
            break;

            case CMD_JOIN:

            break;

            case CMD_PART:

            break;

            case CMD_TOPIC:

            break;

            case CMD_TIME:
            if (args.size() == 1) {
                reply_code = RPL_TIME;
                reply_details = "TIME :"+ get_time();
            } else if (args.size() == 2) {
                std::string target_server = args[1];
                bool server_exists = false;
                for (int i = 0; i < server_list.size(); ++i) {
                    if (server_list[i] == target_server) {
                        server_exists = true;
                    }
                }
                if (server_exists) {
                    reply_code = RPL_TIME;
                    reply_details = target_server + " TIME :" + get_time();
                } else {
                    reply_code = ERR_NOSUCHSERVER; 
                    reply_details = target_server + " :No such server";
                }
            }
            break;

            case CMD_NAMES:
            // if (args.size() < 0) 

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

            reply_message.str("");
        }
    }


    wipe_user(client_ip_addr);
    

    close(sockfd);
    return NULL;
}

void wipe_user(std::string ip_addr_str) {
    int user_idx = users.get_user_idx_via_ip(ip_addr_str);
    if (user_idx != -1) {
        User usr = users.users[user_idx];
        users.remove_user(usr.ip_addr_str);
        channels.remove_user(usr.nickname);
    }
}