#include <cstdio>
#include <cinttypes>
#include "common.h"
#include "err.h"

uint16_t server_port;

void parse_arguments(int argc, char **argv)
{
    if(argc == 2) server_port = PORT_DEFAULT;
    else if(argc == 3 && sscanf(argv[2], "%hu", &server_port) != 1)
        fatal("Invalid server port number");
    else
        fatal("Invalid number of arguments");

}

int main(int argc, char **argv)
{
    //parse_arguments(argc, argv);
    safe_call(-1, "test");


    return 0;
}