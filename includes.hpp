/*
 * Gavin Witsken
 * CS 447
 * Project 1, includes.h
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

//Exit codes
#define EXIT_INVALIDFILEREAD 1


//Error/Reply codes
#define ERR_UNKNOWNCOMMAND 421

#define ERR_NONICKNAMEGIVEN 431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE 433
#define ERR_NICKCOLLISION 436
#define ERR_UNAVAILRESOURCE 437
#define ERR_RESTRICTED 484

#define ERR_NEEDMOREPARAMS 461
#define ERR_ALREADYREGISTRED 462

// #define ERR_NEEDMOREPARAMS 461
#define ERR_BANNEDFROMCHAN 474
#define ERR_INVITEONLYCHAN 473
#define ERR_BADCHANNELKEY 475
#define ERR_CHANNELISFULL 471
#define ERR_BADCHANMASK 476
#define ERR_NOSUCHCHANNEL 403
#define ERR_TOOMANYCHANNELS 405
#define ERR_TOOMANYTARGETS 407
#define ERR_UNAVAILRESOURCE 437
#define RPL_TOPIC 332

// #define ERR_NEEDMOREPARAMS 461
// #define ERR_NOSUCHCHANNEL 403
#define ERR_NOTONCHANNEL 442

// #define ERR_NEEDMOREPARAMS 461
// #define ERR_NOTONCHANNEL 442
#define RPL_NOTOPIC 331
// #define RPL_TOPIC 332
#define ERR_CHANOPRIVSNEEDED 482
#define ERR_NOCHANMODES 477

#define ERR_TOOMANYMATCHES 416
#define ERR_NOSUCHSERVER 402
#define RPL_NAMREPLY 353
#define RPL_ENDOFNAMES 366

#define ERR_NORECIPIENT 411
#define ERR_NOTEXTTOSEND 412
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_NOTOPLEVEL 413
#define ERR_WILDTOPLEVEL 414
// #define ERR_TOOMANYTARGETS 407
#define ERR_NOSUCHNICK 401
#define RPL_AWAY 301

#define ERR_NOSUCHSERVER 402
#define RPL_TIME 391


//Structs/Classes
typedef struct {
    char name[10];
    char perms; //Bitwise combination of permission stuff - specified in RFC 2812
} User;

typedef struct {
    char name[51];
} Channel;

//Functions

int configure_client(std::string filename, std::string& SERVER_IP, std::string& SERVER_PORT) { 
    std::vector<std::string> lines;

    std::ifstream in(filename);
    if (!in.is_open())
        return EXIT_INVALIDFILEREAD;

    while (in >> line) {
        std::string var_name;
        std::string var_val;
        std::stringstream stream;
        stream << line;

        getline(stream, var_name, '=');
        if (stream.fail()) 
            return EXIT_INVALIDFILEREAD;

        getline(stream, var_val);
        if (stream.fail()) 
            return EXIT_INVALIDFILEREAD;

        if (var_name == "SERVER_IP")
            SERVER_IP = var_val;
        else if (var_name == "SERVER_PORT")
            SERVER_PORT = var_val;
        else
            return EXIT_INVALIDFILEREAD;  
    }

    return 0;
}

int configure_server(std::string filename, std::string NICK, std::string PASS, std::string PORT, std::string SERVERS, std::string SOCK_ADDR) { //Returns 0 on success, else failure
    std::vector<std::string> lines;

    std::ifstream in(filename); 
    if (!in.is_open())
        return EXIT_INVALIDFILEREAD;

    while (in >> line) {
        std::string var_name;
        std::string var_val;
        std::stringstream stream;
        stream << line;

        getline(stream, var_name, '=');
        if (stream.fail()) 
            return EXIT_INVALIDFILEREAD;

        getline(stream, var_val);
        if (stream.fail()) 
            return EXIT_INVALIDFILEREAD;

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
        else
            return EXIT_INVALIDFILEREAD;  
    }

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