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
std::vector<std::string> split_by_comma(std::string input);

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
    server_list.push_back(NICK);
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
    users.add_user(client_ip_addr, sockfd, client_hostname);
    
    //Wait for client messages and handle on loop

    while (1) {
        char received_msg_buff[MSGBUFSIZE];
        std::string received_msg;
        bool do_message = true;
        bool do_quit = false;

        if (recv(sockfd, received_msg_buff, sizeof(received_msg_buff), 0) <= 0) {
            std::cerr << "Unable to receive message from \"" << client_ip_addr << "\" or client closed connection" << std::endl;
            break;
        } else {
            received_msg = received_msg_buff;
            std::cout << "Message from client (" << client_ip_addr << "): \"" << received_msg << "\"\n" ;
        }

        //Empty message warrants no repsonse
        if (received_msg == "") {
            continue;
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

                    
                    // std::cerr << "User idx: " << user_idx << std::endl;
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
                            std::string msg = old_nick + "!" + user_info.username + "@" + client_hostname + " " + cmd + " " + nickname;

                            users.broadcast(msg);
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
                    if (user_data.registered) {
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
            if (!users.is_registered(client_ip_addr)) {
                do_quit = true;
                break;
            }
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
                // std::cerr << "Boradcasting message " << msg << std::endl;
                users.broadcast(msg);
            }
            break;


            case CMD_JOIN:
            if (!users.is_registered(client_ip_addr)) {
                reply_code = ERR_NOTREGISTERED;
                reply_details = ":You have not registered";
                break;
            }

            if (args.size() == 1) {
                reply_code = ERR_NEEDMOREPARAMS;
                reply_details = cmd + " :Not enough parameters";
            } else {
                do_message = false;
                //At least 2 params
                //Ignoring key and mask stuff for now
                std::vector<std::string> target_channels = split_by_comma(args[1]);
                // std::cerr << "Target channels:\n";
                for (int i = 0; i < target_channels.size(); ++i) {
                    std::cerr << "\t" << i << ": " << target_channels[i] << std::endl;
                }
                for (int i = 0; i < target_channels.size(); ++i) {
                    std::string channel_name = target_channels[i];

                    if (channel_name == "0") {
                        //Unsubscribe user from all currently subscribed channels
                        channels.remove_user(users.users[users.get_user_idx_via_ip(client_ip_addr)].nickname);
                        //RFC Does not indicate any notification of this to other users in the channel
                    } else if (!is_valid_channame(channel_name)) {
                        reply_code = ERR_NOSUCHCHANNEL;
                        reply_details = channel_name + " :No such channel";
                        std::string msg = ":" + server_name + " " + reply_code + " " + reply_details;
                        std::cerr << "Sending message " << msg << std::endl;
                        if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                            std::cerr << "Error: unable to send reply message" << std::endl;
                            do_quit = true;
                            break;
                        }
                        usleep(100000);
                    } else {
                        //Valid channel name - check if channel exists
                        int channel_idx = channels.get_channel_idx(channel_name);
                        if (channel_idx == -1) {
                            //Need to create channel and add user
                            std::cerr << "Creating channel " << channel_name << std::endl;
                            channels.add_channel(channel_name);
                            channel_idx = channels.get_channel_idx(channel_name);
                            // std::cerr << "Channel idx: " << channel_idx << std::endl;
                            if (channel_idx != -1) {
                                Channel channel = channels.channels[channel_idx];
                                int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                                
                                if (user_idx == -1) {
                                    do_quit = true;
                                    break;
                                } else {
                                    User user = users.users[user_idx];
                                    channels.channels[channel_idx].add_user(user.nickname);

                                    //Send join message to channel members
                                    std::string identifier = user.get_identifier();
                                    std::string msg = ":" + identifier + " JOIN " + channel_name;
                                    channel.send_channel_msg(msg, users);

                                    //Send welcome message to new member
                                    reply_code = RPL_TOPIC;
                                    reply_details = user.nickname + " " + channel_name + " :" + channel.topic;
                                    msg = ":" + server_name + " " + reply_code + " " + reply_details;
                                    std::cerr << "Sending message " << msg << std::endl;
                                    if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                        std::cerr << "Could not send message " << msg << std::endl;
                                        do_quit = true;
                                        break;
                                    }
                                    usleep(100000);

                                    //Send names to new member
                                    channel_idx = channels.get_channel_idx(channel_name);
                                    channel = channels.channels[channel_idx];

                                    reply_code = RPL_NAMREPLY;
                                    reply_details = user.nickname + " = " + channel_name + " :";
                                    for (int j = 0; j < channel.user_nicks.size(); ++j) {
                                        reply_details += channel.user_nicks[j];
                                        if (j < channel.user_nicks.size()-1) {
                                            reply_details += " ";
                                        }
                                    }
                                    msg = msg = ":" + server_name + " " + reply_code + " " + reply_details;
                                    if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                        std::cerr << "Could not send message " << msg << std::endl;
                                        do_quit = true;
                                        break;
                                    }
                                    usleep(100000);

                                    //Send end of names message to new member
                                    reply_code = RPL_ENDOFNAMES;
                                    reply_details = channel_name + " :End of NAMES list";
                                    msg = ":" + server_name + " " + reply_code + " " + reply_details;
                                    std::cerr << "Sending message " << msg << std::endl;
                                    if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                        std::cerr << "Could not send message " << msg << std::endl;
                                        do_quit = true;
                                        break;
                                    }
                                    usleep(100000);
                                }
                            }
                        } else {
                            //Existing channel
                            //Check if user is already in channel
                            channel_idx = channels.get_channel_idx(channel_name);
                            Channel channel = channels.channels[channel_idx];
                            int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                            User user = users.users[user_idx];       
                            if (channel.is_in_channel(user.nickname)) {
                                do_message = true; 
                                reply_code = ERR_USERONCHANNEL;
                                reply_details = user.nickname + " " + channel_name + " :is already on channel";
                                break;
                            }

                            if (channel_idx != -1) {     
                                if (user_idx == -1) {
                                    do_quit = true;
                                    break;
                                } else {
                                    //Need to add user to existing channel
                                    User user = users.users[user_idx];
                                    channels.channels[channel_idx].add_user(user.nickname);
                                    channel = channels.channels[channel_idx];

                                    //Send join message to channel members
                                    std::string identifier = user.get_identifier();
                                    std::string msg = ":" + identifier + " JOIN " + channel_name;
                                    channel.send_channel_msg(msg, users);

                                    //Send welcome message to new member
                                    reply_code = RPL_TOPIC;
                                    reply_details = user.nickname + " " + channel_name + " :" + channel.topic;
                                    msg = ":" + server_name + " " + reply_code + " " + reply_details;
                                    std::cerr << "Sending message " << msg << std::endl;
                                    if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                        std::cerr << "Could not send message " << msg << std::endl;
                                        do_quit = true;
                                        break;
                                    }
                                    usleep(100000);

                                    //Send names to user
                                    channel_idx = channels.get_channel_idx(channel_name);
                                    channel = channels.channels[channel_idx];

                                    reply_code = RPL_NAMREPLY;
                                    reply_details = user.nickname + " = " + channel_name + " :";
                                    for (int j = 0; j < channel.user_nicks.size(); ++j) {
                                        reply_details += channel.user_nicks[j];
                                        if (j < channel.user_nicks.size()-1) {
                                            reply_details += " ";
                                        }
                                    }
                                    msg = ":" + server_name + " " + reply_code + " " + reply_details;
                                    std::cerr << "Sending message " << msg << std::endl;
                                    if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                        std::cerr << "Could not send message " << msg << std::endl;
                                        do_quit = true;
                                        break;
                                    }
                                    usleep(100000);

                                    //Send end of names message to user
                                    reply_code = RPL_ENDOFNAMES;
                                    reply_details = channel_name + " :End of NAMES list";
                                    msg = ":" + server_name + " " + reply_code + " " + reply_details;
                                    std::cerr << "Sending message " << msg << std::endl;
                                    if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                        std::cerr << "Could not send message " << msg << std::endl;
                                        do_quit = true;
                                        break;
                                    }
                                    usleep(100000);
                                }
                            }
                        }
                    }
                }
                
            }
            break;

            case CMD_PART:
            if (!users.is_registered(client_ip_addr)) {
                reply_code = ERR_NOTREGISTERED;
                reply_details = ":You have not registered";
                break;
            }
            if (args.size() < 2) {
                reply_code = ERR_NEEDMOREPARAMS;
                reply_details = " :Not enough parameters";
                break;
            } else {
                do_message = false;

                User user = users.users[users.get_user_idx_via_ip(client_ip_addr)];

                std::vector<std::string> target_channels = split_by_comma(args[1]);

                for (int i = 0; i < target_channels.size(); ++i) {
                    //Go through list of targets
                    std::string channel_name = target_channels[i];

                    int channel_idx = channels.get_channel_idx(channel_name);
                    if (channel_idx == -1) {
                        //Channel does not exist - notify user then continue through list
                        reply_code = ERR_NOSUCHCHANNEL;
                        reply_details = channel_name + " :No such channel";
                        std::string msg = ":" + server_name + " " + reply_code + " " + reply_details;

                        std::cerr << "Sending message " << msg << std::endl;
                        if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                            std::cerr << "Could not send message " << msg << std::endl;
                            do_quit = true;
                            break;
                        }
                        usleep(100000);
                        continue;
                    }
                    
                    Channel channel = channels.channels[channel_idx];

                    if (!channel.is_in_channel(user.nickname)) {
                        //User is not in channel
                        reply_code = ERR_NOTONCHANNEL;
                        reply_details = channel_name + " :You're not on that channel";
                        std::string msg = ":" + server_name + " " + reply_code + " " + reply_details;

                        std::cerr << "Sending message " << msg << std::endl;
                        if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                            std::cerr << "Could not send message " << msg << std::endl;
                            do_quit = true;
                            break;
                        }
                        usleep(100000);
                        continue;
                    }

                    //User is in channel
                    std::string msg = user.get_identifier() + " PART " + channel_name;
                    if (args.size() > 2) {
                        //User has parting message
                        std::string parting_msg = "";
                        for (int j = 2; j < args.size(); ++j) {
                            if (j == 2) {
                                if (args[2][0] != ':') {
                                    parting_msg += ":" + args[2];
                                } else {
                                    parting_msg += args[2];
                                }
                            } else {
                                parting_msg += " " + args[j];
                            }
                        }
                        
                        msg += " " + parting_msg;
                    }
                    channel_idx = channels.get_channel_idx(channel_name);
                    if (channel_idx != -1) {
                        channels.channels[channel_idx].remove_user(user.nickname);
                        channel = channels.channels[channel_idx];
                        channel.send_channel_msg(msg, users);
                    }
                }
            }

            break;

            case CMD_TOPIC:
            if (!users.is_registered(client_ip_addr)) {
                reply_code = ERR_NOTREGISTERED;
                reply_details = ":You have not registered";
                break;
            }
            if (args.size() < 2) {
                reply_code = ERR_NEEDMOREPARAMS;
                reply_details = cmd + " :Not enough parameters";
                break;
            } else {
                int user_idx = users.get_user_idx_via_ip(client_ip_addr);
                if (user_idx == -1) {
                    do_quit = true;
                    break;
                }


                User user = users.users[user_idx];
                //Validate channel name
                std::string channel_name = args[1];
                int channel_idx = channels.get_channel_idx(channel_name);
                Channel channel;
                if (channel_idx == -1) {
                    //Channel does not exist
                    reply_code = ERR_NOSUCHCHANNEL;
                    reply_details = channel_name + " :No such channel";
                    break;
                } else {
                    //Channel exists
                    channel = channels.channels[channel_idx];
                } 

                //Check if user sending request is in channel 
                if (!channel.is_in_channel(user.nickname)) {
                    reply_code = ERR_NOTONCHANNEL;
                    reply_details = channel_name + " :You're not on that channel";
                    break;
                }   
                if (args.size() == 2) {
                    //View topic
                    if (channel.topic == "No topic is set") {
                        reply_code = RPL_NOTOPIC;
                    } else {
                        reply_code = RPL_TOPIC;
                    }
                    reply_details = user.nickname + " " + channel_name + " :" + channel.topic;
                    break;
                } else {
                    //Update topic
                    std::string new_topic = "";
                    if (args.size() == 3 && args[2] == ":") {
                        //Clear channel topic
                        new_topic = "No topic is set";
                    } else {
                        //Have some other topic to set
                        for (int i = 2; i < args.size(); ++i) {
                            if (i == 2) {
                                //first word in new topic
                                if (args[2][0] == ':') {
                                    new_topic += args[2].substr(1,args[2].size()-1);
                                } else {
                                    new_topic += args[2];
                                }
                            } else {
                                new_topic += " " + args[i];
                            }
                        }
                    }
                    //New topic set - need to notify other members of channel
                    channels.channels[channel_idx].topic = new_topic;
                    if (channel.topic == "No topic is set") {
                        reply_code = RPL_NOTOPIC;
                    } else {
                        reply_code = RPL_TOPIC;
                    }
                    std::cerr << "New topic: " << new_topic << std::endl;

                    std::string identifier = user.get_identifier();
                    std::string msg = ":" + identifier + " " + cmd + " " + channel_name + " :" + new_topic; 
                    //Should message users in channel - not just client that requested change
                    do_message = false;

                    channel.send_channel_msg(msg, users);

                    break;
                }
            } 

            break;

            case CMD_TIME:
            if (!users.is_registered(client_ip_addr)) {
                reply_code = ERR_NOTREGISTERED;
                reply_details = ":You have not registered";
                break;
            }
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
            if (!users.is_registered(client_ip_addr)) {
                reply_code = ERR_NOTREGISTERED;
                reply_details = ":You have not registered";
                break;
            }
            do_message = false;
            if (args.size() == 1) {
                //List all users from all channels
                std::vector<std::string> requested_channels;
                std::string msg = "";

                for (int i = 0; i < channels.channels.size(); ++i) {
                    requested_channels.push_back(channels.channels[i].name);
                }

                for (int i = 0; i < requested_channels.size(); ++i) {
                    std::cerr << "\t" << i << ": " << requested_channels[i] << std::endl;
                }

                for (int i = 0; i < requested_channels.size(); ++i) {
                    std::string channel_name = requested_channels[i];
                    std::cerr << "Loop " << i << ": " << requested_channels[i] << std::endl;

                    User user = users.users[users.get_user_idx_via_ip(client_ip_addr)];

                    if (!is_valid_channame(channel_name)) {
                        reply_code = ERR_NOSUCHCHANNEL;
                        reply_details = channel_name + " :No such channel";
                        std::string msg = ":" + server_name + " " + reply_code + " " + reply_details;
                        std::cerr << "Sending message " << msg << std::endl;
                        if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                            std::cerr << "Error: unable to send reply message" << std::endl;
                            do_quit = true;
                            break;
                        }
                        usleep(100000);
                    } else {
                        //Channel name is valid - check if it exists
                        int channel_idx = channels.get_channel_idx(channel_name);

                        if (channel_idx != -1) {
                            //Channel exists - can get members
                            channel_idx = channels.get_channel_idx(channel_name);
                            Channel channel = channels.channels[channel_idx];

                            reply_code = RPL_NAMREPLY;
                            reply_details = user.nickname + " = " + channel_name + " :";
                            for (int j = 0; j < channel.user_nicks.size(); ++j) {
                                reply_details += channel.user_nicks[j];
                                if (j < channel.user_nicks.size()-1) {
                                    reply_details += " ";
                                }
                            }
                            msg = ":" + server_name + " " + reply_code + " " + reply_details;
                            std::cerr << "Sending message " << msg << std::endl;
                            if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                std::cerr << "Could not send message " << msg << std::endl;
                                do_quit = true;
                                break;
                            }
                            usleep(100000);

                            //Send end of names message to new member
                            reply_code = RPL_ENDOFNAMES;
                            reply_details = user.nickname + " " + channel_name + " :End of NAMES list";
                            msg = ":" + server_name + " " + reply_code + " " + reply_details;
                            std::cerr << "Sending message " << msg << std::endl;
                            if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                std::cerr << "Could not send message " << msg << std::endl;
                                do_quit = true;
                                break;
                            }
                            usleep(100000);


                        } else {
                            reply_code = ERR_NOSUCHCHANNEL;
                            reply_details = channel_name + " :No such channel";
                            msg = ":" + server_name + " " + reply_code + " " + reply_details;
                            std::cerr << "Sending message " << msg << std::endl;
                            if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                std::cerr << "Error: unable to send reply message" << std::endl;
                                do_quit = true;
                                break;
                            }
                            usleep(100000);
                        }
                    }
                    std::cerr << "Loop " << i << " end" << std::endl;
                }
            } else {
                //Parse list in args[1] for requested channels
                //Ignoring target server for now
                std::vector<std::string> requested_channels = split_by_comma(args[1]);
                std::string msg = "";
                
                for (int i = 0; i < requested_channels.size(); ++i) {
                    std::cerr << "\t" << i << ": \"" << requested_channels[i] << "\"" << std::endl;
                }

                for (int i = 0; i < requested_channels.size(); ++i) {
                    std::string channel_name = requested_channels[i];

                    std::cerr << "Loop " << i << ": " << requested_channels[i] << std::endl;
                    
                    User user = users.users[users.get_user_idx_via_ip(client_ip_addr)];

                    if (!is_valid_channame(channel_name)) {
                        reply_code = ERR_NOSUCHCHANNEL;
                        reply_details = channel_name + " :No such channel";
                        msg = ":" + server_name + " " + reply_code + " " + reply_details;
                        std::cerr << "Sending message " << msg << std::endl;
                        if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                            std::cerr << "Error: unable to send reply message" << std::endl;
                            do_quit = true;
                            break;
                        }
                        usleep(100000);
                    } else {
                        //Channel name is valid - check if it exists
                        int channel_idx = channels.get_channel_idx(channel_name);

                        if (channel_idx != -1) {
                            //Channel exists - can get members
                            channel_idx = channels.get_channel_idx(channel_name);
                            Channel channel = channels.channels[channel_idx];

                            reply_code = RPL_NAMREPLY;
                            reply_details = user.nickname + " = " + channel_name + " :";
                            for (int j = 0; j < channel.user_nicks.size(); ++j) {
                                reply_details += channel.user_nicks[j];
                                if (j < channel.user_nicks.size()-1) {
                                    reply_details += " ";
                                }
                            }
                            msg = ":" + server_name + " " + reply_code + " " + reply_details;
                            std::cerr << "Sending message " << msg << std::endl;
                            if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                std::cerr << "Could not send message " << msg << std::endl;
                                do_quit = true;
                                break;
                            }
                            usleep(100000);

                            //Send end of names message to new member
                            reply_code = RPL_ENDOFNAMES;
                            reply_details = user.nickname + " " + channel_name + " :End of NAMES list";
                            msg = ":" + server_name + " " + reply_code + " " + reply_details;
                            std::cerr << "Sending message " << msg << std::endl;
                            if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                std::cerr << "Could not send message " << msg << std::endl;
                                do_quit = true;
                                break;
                            }
                            usleep(100000);


                        } else {
                            reply_code = ERR_NOSUCHCHANNEL;
                            reply_details = channel_name + " :No such channel";
                            std::string msg = ":" + server_name + " " + reply_code + " " + reply_details;
                            std::cerr << "Sending message " << msg << std::endl;
                            if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                                std::cerr << "Error: unable to send reply message" << std::endl;
                                do_quit = true;
                                break;
                            }
                            usleep(100000);
                        }
                    }
                    std::cerr << "Loop " << i << " end" << std::endl;
                }

            }

            break;

            case CMD_PRIVMSG:
            if (!users.is_registered(client_ip_addr)) {
                reply_code = ERR_NOTREGISTERED;
                reply_details = ":You have not registered";
                break;
            }

            if (args.size() < 2) {
                //No target
                reply_code = ERR_NORECIPIENT;
                reply_details = ":No recipient given (PRIVMSG)";
                break;
            } else if (args.size() < 3) {
                //No message to send
                reply_code = ERR_NOTEXTTOSEND;
                reply_details = ":No text to send";
                break;
            } else {
                std::string target_name = args[1];
                int target_type = TARGET_USER;
                Channel channel;
                User target_user;

                if (target_name[0] == '#' || target_name[0] == '&' || target_name[0] == '+' || target_name[0] == '!') {
                    //target is a channel
                    int channel_idx = channels.get_channel_idx(target_name);
                    if (channel_idx == -1) {
                        //channel does not exist
                        reply_code = ERR_NOSUCHCHANNEL;
                        reply_details = target_name + " :No such channel";
                        break;
                    }
                    channel = channels.channels[channel_idx];
                    target_type = TARGET_CHANNEL;
                } else {
                    //target is a user
                    int user_idx = users.get_user_idx_via_nick(target_name);
                    if (user_idx == -1) {
                        reply_code = ERR_NOSUCHNICK;
                        reply_details = target_name + " :No such nick";
                        break;
                    }
                    target_user = users.users[user_idx];
                    target_type = TARGET_USER;
                }

                do_message = false;
                
                std::string usr_msg = "";
                for (int i = 2; i < args.size(); ++i) {
                    if (i == 2) {
                        if (args[2][0] != ':') {
                            usr_msg += ":" + args[2];
                        } else {
                            usr_msg += args[2];
                        }
                    } else {
                        usr_msg += " " + args[i];
                    }
                }

                User user = users.users[users.get_user_idx_via_ip(client_ip_addr)];
                std::string msg = user.get_identifier() + " " + cmd + " " + target_name + " " + usr_msg;
 
                if (target_type == TARGET_USER) {
                    //send msg to user
                    std::cerr << "Sending message " << msg << std::endl;
                    if (send(target_user.sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                        std::cerr << "Error: unable to send message to target" << std::endl;
                        break;
                    }
                    usleep(100000);
                } else {
                    //send msg to channel
                    channel.send_channel_msg(msg, users);
                }
            }

            break;

            case CMD_INVALID:
            default:
            reply_code = ERR_UNKNOWNCOMMAND;
            reply_details = cmd + " :Unknown command";

            break;
        }

        if (do_quit) {
            break;
        }

        // std::cerr << "Made it through switch statement" << std::endl;
        
        if (do_message) {
            //Put message components together
            reply_message << ":" << server_name << " " << reply_code << " " << reply_details;
            std::string reply_message_str = reply_message.str();
            
            std::cerr << "Sending reply message: \"" << reply_message_str << "\"" << std::endl;

            //Send message - if failed, terminate the connection and thread
            if (send(sockfd, reply_message_str.c_str(), reply_message_str.size() + 1, 0) <= 0) {
                std::cerr << "Error: unable to send reply message" << std::endl;
                break;
            } else {
                // std::cout << "Sent message to client (" << client_ip_addr << "): \"" << reply_message_str << "\"" << std::endl;
            }
            usleep(100000);

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

std::vector<std::string> split_by_comma(std::string input) {
    std::stringstream stream(input);
    std::string word;
    std::vector<std::string> words;
    while (getline(stream, word, ',')) {
        words.push_back(word);
    }

    return words;
}