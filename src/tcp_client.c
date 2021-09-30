#include "log.h"

#include "tcp_client.h"

#include <sys/types.h>
#include <sys/socket.h>

/*
print message to clarify usage of the commandline tool
*/
void tcp_client_printHelpMessage() {
    printf("\nUsage: tcp_client [--help] [-v] [-h HOST] [-p PORT] ACTION MESSAGE\n\n");
    printf("Arguments:\n  ACTION\tMust be uppercase, lowercase, title-case,\n\t\treverse, or shuffle.\n  MESSAGE\tMessage to send to the server\n");
    printf("Options:\n  --help\n  -v, --verbose\n  --host HOSTNAME, -h HOSTNAME\n  --port PORT, -p PORT\n");
}

// allocate memory for the configuration details
void tcp_client_allocate_config(Config *config) {
    config->host = malloc(sizeof(char)*(100+1));
    config->port = malloc(sizeof(char)*(5+1));
    config->file = malloc(sizeof(char)*(100+1));
    log_trace("memory allocated");
}

/*
Description:
    Parses the commandline arguments and options given to the program.
Arguments:
    int argc: the amount of arguments provided to the program (provided by the main function)
    char *argv[]: the array of arguments provided to the program (provided by the main function)
    Config *config: An empty Config struct that will be filled in by this function.
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_parse_arguments(int argc, char *argv[], Config *config) {
    int c;

    tcp_client_allocate_config(config);

    strcpy(config->port, TCP_CLIENT_DEFAULT_PORT);
    strcpy(config->host, TCP_CLIENT_DEFAULT_HOST);
    log_trace("enter parse args");
    while(1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"help", no_argument, NULL, 0},
            {"verbose", no_argument, NULL, 'v'},
            {"host", required_argument, NULL, 'h'},
            {"port", required_argument, NULL, 'p'},
            {NULL, 0, NULL, 0}
        };
        c = getopt_long(argc, argv, "vh:p:", long_options, &option_index);
        if(c == -1) {
            break;
        }

        switch (c) {
            case 0:
                tcp_client_printHelpMessage();
                break;
            case 'v':
                log_set_level(LOG_TRACE);
                break;
            case 'h':
                log_info("host: %s", optarg);
                strcpy(config->host, optarg);
                break;
            case 'p':
                log_info("port: %s", optarg);
                strcpy(config->port, optarg);
                break;
            case '?':
                log_info("unrecognized option, exiting program");
                tcp_client_printHelpMessage();
                return 1;
        }
    }
    if (optind < argc) {
        config->file = argv[optind];
        log_info("File: %s", config->file);    
    }

    else {
        log_debug("Incorrect number of arguments");
        tcp_client_printHelpMessage();
        return 1;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////
/////////////////////// SOCKET RELATED FUNCTIONS //////////////////////
///////////////////////////////////////////////////////////////////////

/*
Description:
    Creates a TCP socket and connects it to the specified host and port.
Arguments:
    Config config: A config struct with the necessary information.
Return value:
    Returns the socket file descriptor or -1 if an error occurs.
*/
int tcp_client_connect(Config config) {
    struct addrinfo hints, *servinfo, *p;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(config.host, config.port, &hints, &servinfo);

    log_info("Searching for a socket to connect to");
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
                continue;
        }
        log_info("Attempting to connect to a socket");
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (p == NULL) {
        log_debug("No available sockets to connect to");
        return -1;
    }
    log_info("Connected to a socket");
    return sockfd;
}

/*
Helper function that assures that all data is sent before returning
*/
int sendAll(int sockfd, char *action, char *message) {
    
    int len, sent;
    char *msg = malloc(strlen(action)+4+strlen(message));
    char str[5];
    sprintf(str, "%lu", strlen(message));
    strcpy(msg, action);
    strcat(msg, " ");
    strcat(msg, str);
    strcat(msg, " ");
    strcat(msg, message);
    len = strlen(msg);

    log_info("Message being sent to the server: %s", msg);
    sent = send(sockfd, msg, len, 0);
    if(sent == -1) {
        log_warn("Data was not successfully sent to the server");
        return -1;
    }
    return 0;
}

/*
Description:
    Creates and sends request to server using the socket and configuration.
Arguments:
    int sockfd: Socket file descriptor
    Config config: A config struct with the necessary information.
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_send_request(int sockfd, char *action, char *message) {
    log_info("Sending data to the server");
    return sendAll(sockfd, action, message);
}

/*
Description:
    Receives the response from the server. The caller must provide a function pointer that handles
the response and returns a true value if all responses have been handled, otherwise it returns a
    false value. After the response is handled by the handle_response function pointer, the response
    data can be safely deleted. The string passed to the function pointer must be null terminated.
Arguments:
    int sockfd: Socket file descriptor
    int (*handle_response)(char *): A callback function that handles a response
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_receive_response(int sockfd, int (*handle_response)(char *)) {
    char * space_position = 0;
    int last_message = 0;
    size_t numBytesInBuffer = 0;
    size_t bufferSize = 1024;
    char *buffer;
    int messageLength = 0;
    
    char *response_message = NULL;

    buffer = malloc(sizeof(char)*bufferSize);
    buffer[0] = '\0';
    // while there are more messages to be received, 
    // receive and process the messages
    log_trace("Beginning to receive messages.");
    while(!last_message) {
        //allocate buffer space
        if(numBytesInBuffer > bufferSize/2) {
            bufferSize *= 2;
            buffer = realloc(buffer, bufferSize);
        }
        log_info("Continue receiving data %d", sockfd);

        int numbytes = recv(sockfd, buffer+numBytesInBuffer, bufferSize-numBytesInBuffer-1, 0);
        numBytesInBuffer += numbytes;
        buffer[numBytesInBuffer] = '\0';
        log_info("Number of bytes in the buffer: %zu", numBytesInBuffer);

        while((space_position = strchr(buffer, ' ')) != NULL) {
            log_info("Contents of buffer: %s", buffer);
            log_info("Not me");
            sscanf(buffer, "%d", &messageLength);
            log_info("Whats up dog");
            log_info("Message length is: %zu", messageLength);
            if(numBytesInBuffer - (space_position+1-buffer)) {
                response_message = malloc(sizeof(char)*(messageLength+1));
                strncpy(response_message, space_position+1, messageLength);
                response_message[messageLength] = '\0';
                last_message = handle_response(response_message);
                free(response_message);
                numBytesInBuffer -= (space_position-buffer)+1+messageLength;
                log_trace("New number of bytes in buffer: %zu", numBytesInBuffer);
                log_info("Buffer: %s, New buffer: %s", buffer, space_position+1+messageLength);
                char *tempString = malloc(sizeof(char)*(numBytesInBuffer+1));
                strcpy(tempString, space_position+1+messageLength);
                strcpy(buffer, tempString);
                free(tempString);
                log_info("Got it");
            }
        }
    }
    free(buffer);
    return 0;
}

/*
Description:
    Closes the given socket.
Arguments:
    int sockfd: Socket file descriptor
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_close(int sockfd) {
    log_info("Closing the socket connection");
    if(close(sockfd) == -1) {
        log_warn("Failed to close connection");
        return 1;
    }
    log_info("Successfully closed socket");
    return 0;
}

/*
Description:
    Opens a file.
Arguments:
    char *file_name: The name of the file to open
Return value:
    Returns NULL on failure, a FILE pointer on success
*/
FILE *tcp_client_open_file(char *file_name) {
    log_info("File name: %s", file_name);
    if(strcmp(file_name,"-") == 0) {
        return stdin;
    }
    return fopen(file_name, "r");
}

/*
Description:
    Gets the next line of a file, filling in action and message. This function should be similar
    design to getline() (https://linux.die.net/man/3/getline). *action and message must be allocated
    by the function and freed by the caller.* When this function is called, action must point to the
    action string and the message must point to the message string.
Arguments:
    FILE *fd: The file pointer to read from
    char **action: A pointer to the action that was read in
    char **message: A pointer to the message that was read in
Return value:
    Returns -1 on failure, the number of characters read on success
*/
int tcp_client_get_line(FILE *fd, char **action, char **message) {

    *action = malloc(sizeof(char)*(1024));
    *message = malloc(sizeof(char)*(1024));
    char *stringLine = NULL;
    size_t readIn = 1024;
    ssize_t charCount;

    if((charCount = getline(&stringLine, &readIn, fd)) == -1) {
        log_info("No line was read from file, program likely reached the end of the file.");
        return -1;
    }
    stringLine[charCount-1] = '\0';
    log_trace("String read from the file is: %s", stringLine);
    int read = sscanf(stringLine, "%s %[^\n]", *action, *message);
    return read;
}

/*
Description:
    Closes a file.
Arguments:
    FILE *fd: The file pointer to close
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_close_file(FILE *fd) {
    return fclose(fd);
}
