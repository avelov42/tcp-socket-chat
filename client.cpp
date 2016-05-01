#include <stdint.h>
#include <stdio.h>
#include "err.h"

uint16_t server_port;


void parse_arguments(int argc, char **argv)
{
    if(argc >= 2)
    {
        ///get host name

    }
    else
        fatal("Invalid number of arguments");

    if(argc == 3 && sscanf(argv[2], "%hu", &server_port) != 1)
        fatal("Invalid server port number");
    else
        fatal("Invalid number of arguments");
}


int main(int argc, char** argv)
{
    parse_arguments(argc, argv);
}
