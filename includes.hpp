/*
 * Gavin Witsken
 * CS 447
 * Project 1, includes.hpp
 * 03/13/2023
*/

//C libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

//CPP libraries (use namespace std)
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <regex>
#include <chrono>
#include <ctime>

//Macros/Constants
#define CMD_INVALID -1

#define CMD_NICK 0
#define CMD_USER 1
#define CMD_QUIT 2
#define CMD_JOIN 3
#define CMD_PART 4
#define CMD_TOPIC 5
#define CMD_NAMES 6
#define CMD_PRIVMSG 7
#define CMD_TIME 8

#define MSGBUFSIZE 1024

//Exit codes
#define EXIT_INVALIDFILEREAD 1
#define EXIT_SOCKET 2
#define EXIT_ARGCOUNT 3
#define EXIT_BIND 4
#define EXIT_LISTEN 5
#define EXIT_CONNECT 6

//Error/Reply codes
#define ERR_UNKNOWNCOMMAND "421"
#define RPL_WELCOME "001"

#define ERR_NONICKNAMEGIVEN "431"
#define ERR_ERRONEUSNICKNAME "432"
#define ERR_NICKNAMEINUSE "433" 
#define ERR_NICKCOLLISION "436" //Only one server here, not used
#define ERR_UNAVAILRESOURCE "437" //No nick delay mechanism, not used
#define ERR_RESTRICTED "484" //Not implementing user modes, not used

#define ERR_NEEDMOREPARAMS "461"
#define ERR_ALREADYREGISTRED "462"
#define ERR_ERRONEUSUSERNAME "5555" //New code implemented by me
#define ERR_ERRONEOUSREALNAME "5556" //New code implemented by me

// #define ERR_NEEDMOREPARAMS "461"
#define ERR_BANNEDFROMCHAN "474"
#define ERR_INVITEONLYCHAN "473"
#define ERR_BADCHANNELKEY "475"
#define ERR_CHANNELISFULL "471"
#define ERR_BADCHANMASK "476"
#define ERR_NOSUCHCHANNEL "403"
#define ERR_TOOMANYCHANNELS "405"
#define ERR_TOOMANYTARGETS "407"
#define ERR_UNAVAILRESOURCE "437"
#define RPL_TOPIC "332"

// #define ERR_NEEDMOREPARAMS 461
// #define ERR_NOSUCHCHANNEL 403
#define ERR_NOTONCHANNEL "442"

// #define ERR_NEEDMOREPARAMS 461
// #define ERR_NOTONCHANNEL 442
#define RPL_NOTOPIC "331"
// #define RPL_TOPIC 332
#define ERR_CHANOPRIVSNEEDED "482"
#define ERR_NOCHANMODES "477"

#define ERR_TOOMANYMATCHES "416"
#define ERR_NOSUCHSERVER "402"
#define RPL_NAMREPLY "353"
#define RPL_ENDOFNAMES "366"

#define ERR_NORECIPIENT "411"
#define ERR_NOTEXTTOSEND "412"
#define ERR_CANNOTSENDTOCHAN "404"
#define ERR_NOTOPLEVEL "413"
#define ERR_WILDTOPLEVEL "414"
// #define ERR_TOOMANYTARGETS 407
#define ERR_NOSUCHNICK "401"
#define RPL_AWAY "301"

#define ERR_NOSUCHSERVER "402"
#define RPL_TIME "391"


//Structs/Classes
class User{
    public:
    std::string nickname;
    std::string username;
    std::string realname;
    // char perms; not implementing permissions here, but normally would be
    std::string ip_addr_str;
    bool registered;
    std::vector<std::string> channels;
    int sockfd;

    User() {
        nickname = "";
        username = "";
        realname = "";
        ip_addr_str = "";
        registered = false;
        sockfd = -1;
    }
    User(std::string ip_addr_str, int sockfd) {
        nickname = "";
        username = "";
        realname = "";
        this->ip_addr_str = ip_addr_str;
        registered = false;
        this->sockfd = sockfd;
    }
};

class Channel {
    public:
    std::string name;
    std::vector<std::string> user_nicks;
    bool is_in_channel(std::string nick) {
        for (int i = 0; i < user_nicks.size(); ++i) {
            if (user_nicks[i] == nick) {
                return true;
            }
        }
        return false;
    }

    void add_user(std::string nick) {
        user_nicks.push_back(nick);
    }
    void remove_user(std::string nick) {
        int user_idx = -1;
        for (int i = 0; i < user_nicks.size(); ++i) {
            if (user_nicks[i] == nick) {
                user_idx = i;
                break;
            }
        }
        if (user_idx != -1) {
            user_nicks.erase(user_nicks.begin() + user_idx);
        }
    }
};

typedef struct {
    struct sockaddr_in addr;
    int sockfd;
} Client_Info;

//Functions

class Channel_List {
    public:
    std::vector<Channel> channels;

    void add_channel(std::string channel_name) {
        Channel channel;
        channel.name = channel_name;
        channels.push_back(channel);
    }
    bool channel_exists(std::string channel_name) {
        for (int i = 0; i < channels.size(); ++i) {
            if (channels[i].name == channel_name) {
                return true;
            }
        }
        return false;
    }
    bool is_user_in_channel(std::string user_nick, std::string channel_name) {
        
        for (int i = 0; i < channels.size(); ++i) {
            if (channels[i].name == channel_name) {
                return channels[i].is_in_channel(user_nick);
            }
        }
        return false;
    }

    void add_user_to_channel(std::string user_nick, std::string channel_name) {
        for (int i = 0; i < channels.size(); ++i) {
            if (channels[i].name == channel_name) {
                channels[i].add_user(user_nick);
            }
        }
    }

    void remove_user(std::string user_nick) {
        for (int i = 0; i < channels.size(); ++i) {
            channels[i].remove_user(user_nick);
        }
    }
    
};

class User_List {
    public:
    std::vector<User> users;
    void add_user(std::string ip_addr_str, int sockfd) {
        User usr;
        usr.nickname = "";
        usr.username = "";
        usr.ip_addr_str = ip_addr_str;
        usr.sockfd = sockfd;

        users.push_back(usr);
    }
    int update_nick(std::string ip_addr_str, std::string new_nick) {
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->ip_addr_str == ip_addr_str) {
                iter->nickname = new_nick;
                return 0;
            }
        }
        return 1;
    }
    int update_user(std::string ip_addr_str, std::string new_user) {
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->ip_addr_str == ip_addr_str) {
                iter->username = new_user;
                return 0;
            }
        }
        return 1;
    }
    int remove_entry(std::string ip_addr_str);
    int update_real(std::string ip_addr_str, std::string new_real) {
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->ip_addr_str == ip_addr_str) {
                iter->realname = new_real;
                return 0;
            }
        }
        return 1;
    }

    bool nick_exists(std::string nick) {
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->nickname == nick) {
                return true;
            }
        }
        return false;
    }

    int get_user_idx_via_nick(std::string nickname) {
        int i = 0;
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->nickname == nickname) {
                std::cerr << "cur_nick: \"" << iter->nickname << "\"" << std::endl; 
                return i;
            }
            ++i;
        }

        return -1;
    }
    int get_user_idx_via_ip(std::string ip_addr_str) {
        int i = 0;
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->ip_addr_str == ip_addr_str) {
                return i;
            }
            ++i;
        }

        return -1;
    }
    void broadcast(std::string msg) {
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            int sockfd = iter->sockfd;
            if (iter->registered) {
                if (send(sockfd, msg.c_str(), msg.size()+1, 0) <= 0) {
                    std::cerr << "Unable to send message to " << iter->nickname << std::endl;
                }
            }
        }
    }
    void remove_user(std::string ip_addr_str) {
        int i = 0;
        for (std::vector<User>::iterator iter = users.begin(); iter < users.end(); iter++) {
            if (iter->ip_addr_str == ip_addr_str) {
                users.erase(users.begin() + i);
            }
            ++i;
        }
    }
};

int configure_client(std::string filename, std::string& SERVER_IP, std::string& SERVER_PORT) { 
    std::string line;
    std::string var_name = "";
    std::string var_val = "";

    std::ifstream in(filename);
    if (!in.is_open())
        return EXIT_INVALIDFILEREAD;

    while (in >> line) {
        var_name = "";
        var_val = "";

        var_name = line.substr(0, (line.find("=")));
        var_val = line.substr( (line.find("=") + 1), std::string::npos);
        std::cout << var_name << std::endl << var_val << std::endl;

        if (var_name == "SERVER_IP")
            SERVER_IP = var_val;
        else if (var_name == "SERVER_PORT")
            SERVER_PORT = var_val;
        else {
            std::cerr << "Error: Unknown var_name \"" << var_name << "\"" << std::endl;
            in.close();
            return EXIT_INVALIDFILEREAD;  
        }
    }

    in.close();
    return 0;
}

int configure_server(std::string filename, std::string& NICK, std::string& PASS, std::string& PORT, std::string& SERVERS, std::string& SOCK_ADDR) { //Returns 0 on success, else failure
    std::string line;
    std::string var_name = "";
    std::string var_val = "";

    std::ifstream in(filename); 
    if (!in.is_open())
        return EXIT_INVALIDFILEREAD;

    while (in >> line) {
        var_name = "";
        var_val = "";
        
        var_name = line.substr(0, (line.find("=")));
        var_val = line.substr( (line.find("=") + 1), std::string::npos);
        // std::cout << "var_name: " << var_name << "\nvar_val: " << var_val << std::endl;

        if (var_name == "NICK")
            NICK = var_val;
        else if (var_name == "PASS")
            PASS = var_val;
        else if (var_name == "PORT")
            PORT = var_val;
        else if (var_name == "SERVERS")
            SERVERS = var_val;
        else if (var_name == "SOCK_ADDR")
            SOCK_ADDR = var_val;
        else {
            in.close();
            return EXIT_INVALIDFILEREAD;  
        }
    }

    in.close();
    return 0;
}

int get_command_key(std::string command) { //Returns -1 on failure, else returns key for command to be used in switch statement
    if (command == "NICK")
        return CMD_NICK;
    else if (command == "USER")
        return CMD_USER;
    else if (command == "QUIT")
        return CMD_QUIT;
    else if (command == "JOIN")
        return CMD_JOIN;
    else if (command == "PART")
        return CMD_PART;
    else if (command == "TOPIC")
        return CMD_TOPIC;
    else if (command == "NAMES")
        return CMD_NAMES;
    else if (command == "PRIVMSG")
        return CMD_PRIVMSG;
    else if (command == "TIME")
        return CMD_TIME;
    else
        return CMD_INVALID;
}

int make_stream_socket() { //Returns file descriptor to created stream socket; returns -1 on failure
    return socket(PF_INET, SOCK_STREAM, 0); //IPv4, Stream, TCP
}

void server_addr_init(int port, struct sockaddr_in* server_address) {
    server_address->sin_family = AF_INET;
    server_address->sin_port = htons(port);     // short, network byte order
    server_address->sin_addr.s_addr = INADDR_ANY;
    memset(server_address->sin_zero, '\0', sizeof(server_address->sin_zero));
}

std::vector<std::string> tokenize(std::string input) {
    std::stringstream stream(input);
    std::string word;
    std::vector<std::string> words;
    while (getline(stream, word, ' ')) {
        words.push_back(word);
    }

    return words;
}

bool is_valid_nickname(std::string nickname) {
    if (nickname.size() > 9 || nickname.size() <= 0) {
        return false;
    }

    for (int i = 0; i < nickname.size(); i++) {
        char c = nickname[i];

        if (!isalpha(c) && !isdigit(c) && c != '\\' && c != '-' && c != '_' && c != '`' && c != '{' && c != '}' && c != '[' && c != ']' && c != '|') {
            //character is invalid
            return false;
        }
    }

    return true;
}

bool is_valid_username(std::string username) {
    if (username.size() > 9 || username.size() <= 0) {
        return false;
    }

    for (int i = 0; i < username.size(); i++) {
        char c = username[i];

        if (!isalpha(c) && !isdigit(c) && c != '\\' && c != '-' && c != '_' && c != '`' && c != '{' && c != '}' && c != '[' && c != ']' && c != '|') {
            //character is invalid
            return false;
        }
    }

    return true;
}

bool is_valid_realname(std::string realname) {
    if (realname.size() > 30 || realname.size() <= 0) {
        return false;
    }

    for (int i = 0; i < realname.size(); i++) {
        char c = realname[i];

        // std::cerr << "Trying to validate realname: " << realname << std::endl;

        if (!isalpha(c) && !isdigit(c) && c != '\\' && c != '-' && c != '_' && c != '`' && c != '{' && c != '}' && c != '[' && c != ']' && c != '|' && c != ' ') {
            //character is invalid
            std::cerr << "\nInvalidating character: \"" << c << "\"" << std::endl;
            return false;
        }
    }

    return true;
}

std::string get_time() {
    std::chrono::system_clock::time_point time = std::chrono::system_clock::now();
    std::time_t cur_time = std::chrono::system_clock::to_time_t(time);

    std::stringstream stream;
    stream << std::ctime(&cur_time);
    return stream.str();
}