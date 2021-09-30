#include <stdio.h>

#include "tcp_client.h"
#include "log.h"

static size_t messages_sent = 0;
static size_t messages_received = 0;

void printHelpMessage() {
    printf("\nUsage: tcp_client [--help] [-v] [-h HOST] [-p PORT] ACTION MESSAGE\n\n");
    printf("Arguments:\n  ACTION\tMust be uppercase, lowercase, title-case,\n\t\treverse, or shuffle.\nMESSAGE\tMessage to send to the server\n");
    printf("Options:\n  --help\n  -v, --verbose\n  --host HOSTNAME, -h HOSTNAME\n  --port PORT, -p PORT\n");
}

int handle_response(char *response) {
    printf("%s\n", response);
    messages_received++;
    if(messages_sent > messages_received) {
        return 0;
    }
    else {
        return 1;
    }
} 

int main(int argc, char *argv[]) {

    Config conf;
    int socket;

    log_set_level(LOG_ERROR);

    int result = tcp_client_parse_arguments(argc, argv, &conf);
    if(result != 0) {
        log_warn("Incorrect arguments provided");
        printHelpMessage();
        exit(EXIT_FAILURE);
    }


    log_info("host: %s, port: %s", conf.host, conf.port);


    socket = tcp_client_connect(conf);
    if(socket == -1) {
        log_warn("Unable to connect to a socket, exiting program");
        exit(EXIT_FAILURE);
    }
    else {
        log_trace("Connection was established to the socket.");
    }

    // open the file that will be read from
    FILE *f = tcp_client_open_file(conf.file);
    log_info("Contents of file descriptor %d.", f);

    //erorr checking on the file connection
    if(f != NULL) {
        log_trace("File was successfully opened.");
    }
    else {
        log_error("There was an error trying to open the file.");
    }


    char *action;
    char *message;
    ssize_t c;

    // while there is data in the file to be sent, 
    // get data from the file and send it to the server
    while((c = tcp_client_get_line(f, &action, &message)) != -1) {
        
        log_trace("Attempting to send a new send message with action: %s, and message: %s.", action, message);
        if(tcp_client_send_request(socket, action, message)) {
            log_warn("Message was not sent successfully to the server");
            exit(EXIT_FAILURE);
        }
        else {
            messages_sent++;
        }
    }
    if((c == -1) && (messages_sent == 0)) {
        log_warn("No messages were sent.");
    }

    // while there are messages we've sent and have not received a response for, 
    // keep receiving from the server
    log_info("Messages sent: %d, messages received: %d.", messages_sent, messages_received);
    // int (*handle_pointer)(char *) = &handle_response;
    tcp_client_receive_response(socket, handle_response);
    
  
    
    if(tcp_client_close_file(f)) {
        log_error("There was a problem trying to close the file.");
    }
    else {
        log_trace("File was closed successfully.");
    }


    if(tcp_client_close(socket)) {
        log_warn("Unable to disconnect from the server");
        exit(EXIT_FAILURE);
    }

    log_info("Program executed successfully");
    exit(EXIT_SUCCESS);
}
