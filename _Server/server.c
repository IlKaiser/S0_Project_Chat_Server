#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>

#include "server.h"


/* Memory shared between all threads */

//Connection Arrays
char* user_names[MAX_SIZE];
int sockets[MAX_SIZE];

//Previous size of connection arrays
int previous_size=0;
//Current size of connection arrays
int current_size=0;
//Semaphore for mutual exclusion
sem_t sem;


int main(int argc, char* argv[]) {
    int ret;

    int socket_desc;

    //init semaphore
    ret=sem_init(&sem,0,1);
    if(ret){
        handle_error("Error creating semaphore");
    }    


    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

     // we will reuse it for accept()

    // initialize socket for listening
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc < 0) handle_error("Could not create socket");

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

    /* We enable SO_REUSEADDR to quickly restart our server after a crash:
     * for more details, read about the TIME_WAIT state in the TCP protocol */
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    if (ret) handle_error("Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    if (ret) handle_error("Cannot bind address to socket");

    printf("%s","Server Online\n");
    fflush(stdout);


    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);
    if (ret) handle_error("Cannot listen on socket");

    

    mthreadServer(socket_desc);


    exit(EXIT_SUCCESS); // this will never be executed
}
void connection_handler(int socket_desc, struct sockaddr_in* client_addr) {
    int ret, recv_bytes,bytes_sent;

    char buf[1024];
    size_t buf_len = sizeof(buf);
    int msg_len;

    char* quit_command = SERVER_COMMAND;
    char  user_name[32];
    size_t quit_command_len = strlen(quit_command);

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

    #ifdef DEBUG
        printf("Client ip %s, client port %hu",client_ip,client_port);
    #endif // DEBUG
    // get client username
    memset(user_name, 0, sizeof(user_name));
        recv_bytes = 0;
        do {
            ret = recv(socket_desc, user_name + recv_bytes, 1, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot read from the socket");
            if (ret == 0) break;
	} while ( user_name[recv_bytes++] != '\n' );

    #ifdef DEBUG
        printf("Username get: %s\n",user_name);
    #endif // DEBUG

    //Updates global variables info
    if(previous_size==MAX_SIZE){
        strcpy(buf,ERROR_MSG);
        bytes_sent = 0;
        msg_len = strlen(buf);
	    while ( bytes_sent < msg_len){
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot write to the socket");
            bytes_sent += ret;
        }
        ret = close(socket_desc);
        if (ret) handle_error("Cannot close socket for incoming connection");
        return;
    }else{
        //Critical section shared mem writing
        //updates shared array
        ret=sem_wait(&sem);
        if(ret){
            handle_error("Err sem wait");
        }
        previous_size=current_size;
        user_names[current_size]=user_name;
        sockets[current_size]=socket_desc;
        current_size++;
        ret=sem_post(&sem);
        if(ret){
            handle_error("Err sem post");
        }
        //send ack to client
        strcpy(buf,OK_MSG);
        bytes_sent = 0;
        msg_len = strlen(buf);
	    while ( bytes_sent < msg_len){
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot write to the socket");
            bytes_sent += ret;
        }
    }



    ///TODO: apre la connessione con il DB


    // check if there is only one client
    while(current_size<2){
        strcpy(buf,ALONE_MSG);
        msg_len = strlen(buf);
        bytes_sent = 0;
	    while ( bytes_sent < msg_len) {
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot write to the socket");
            bytes_sent += ret;
        }
        ret=sleep(5);
        if(ret){
            handle_error("Err sleep");
        }
    }
    // send user list
    int i;
    for(i=0;i<current_size;i++){
        list_formatter(i,buf);
    }
    msg_len = strlen(buf);
    bytes_sent = 0;
	while ( bytes_sent < msg_len) {
        ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) handle_error("Cannot write to the socket");
        bytes_sent += ret;
    }
    // get id number from client
    int user_id;
    do {
            ret = recv(socket_desc, &user_id + recv_bytes, 1, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot read from the socket");
            if (ret == 0) break;
            recv_bytes++;
		}while ( recv_bytes < sizeof(int) );
    #ifdef DEBUG
        printf("User id chosen: %d\n",user_id);
    #endif // DEBUG

    ///TODO: manda l'ack (stesso id) se manda stesso id significa che è ancora in lista altrimenti errore(0xAFFAF)

    int socket_target=sockets[user_id];

    if(socket_target){
        strcpy(buf,OK_MSG);
        bytes_sent = 0;
        msg_len = strlen(buf);
        while ( bytes_sent < msg_len){
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot write to the socket");
            bytes_sent += ret;
        }
    }else{
        strcpy(buf,ERROR_MSG);
        bytes_sent = 0;
        msg_len = strlen(buf);
        while ( bytes_sent < msg_len){
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot write to the socket");
            bytes_sent += ret;
        }
    }

    // reciver loop 
    while (1) {
        // read message from client
        memset(buf, 0, buf_len);
        recv_bytes = 0;
        do {
            ret = recv(socket_desc, buf + recv_bytes, 1, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot read from the socket");
            if (ret == 0) break;
		} while ( buf[recv_bytes++] != '\n' );
        // check whether I have just been told to quit...
        if (recv_bytes == 0) break;
        if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        // ... or if I have to send the message back
        ///TODO: mette nel db il messaggio

        // send to requested target
        bytes_sent = 0;
        msg_len = strlen(buf);
        while ( bytes_sent < msg_len){
            ret = send(socket_target, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            if (ret == -1) handle_error("Cannot write to the socket");
            bytes_sent += ret;
        }
    }
    
    // close socket
    

    //Critical section shared mem writing
    //remove shared array
    ret=sem_wait(&sem);
    if(ret){
        handle_error("Err sem wait");
    }
    previous_size=current_size;
    current_size--;
    ret=sem_post(&sem);
    if(ret){
        handle_error("Err sem post");
    }
    ret = close(socket_desc);
    if (ret) handle_error("Cannot close socket for incoming connection");
}




// Wrapper function that take as input handler_args_t struct and then call 
// connection_handler.
void *thread_connection_handler(void *arg) {
    handler_args_t *args = (handler_args_t *)arg;
    int socket_desc = args->socket_desc;
    struct sockaddr_in *client_addr = args->client_addr;
    connection_handler(socket_desc,client_addr);
    // do not forget to free client_addr! by design it belongs to the
    // thread spawned for handling the incoming connection
    free(args->client_addr);
    free(args);
    pthread_exit(NULL);
}
// function that takes handler_args_m and 
void *thread_message_handler(void *arg){
    return NULL;
}

void message_handler(){}

void mthreadServer(int server_desc) {
    int ret = 0;

    // loop to manage incoming connections spawning handler threads
    int sockaddr_len = sizeof(struct sockaddr_in);
    while (1) {
        // we dynamically allocate a fresh client_addr object at each
        // loop iteration and we initialize it to zero
        struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));
        
        // accept incoming connection
        int client_desc = accept(server_desc, (struct sockaddr *) client_addr, (socklen_t *)&sockaddr_len);
        
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        if (client_desc < 0) handle_error("Cannot open socket for incoming connection");
        
        if (DEBUG) fprintf(stderr, "Incoming connection accepted...\n");

        pthread_t thread;

        // prepare arguments for the new thread
        handler_args_t *thread_args = malloc(sizeof(handler_args_t));
        thread_args->socket_desc = client_desc;
        thread_args->client_addr = client_addr;
        ret = pthread_create(&thread, NULL, thread_connection_handler, (void *)thread_args);
        if (ret) handle_error_en(ret, "Could not create a new thread");
        
        if (DEBUG) fprintf(stderr, "New thread created to handle the request!\n");
        
        ret = pthread_detach(thread); // I won't phtread_join() on this thread
        if (ret) handle_error_en(ret, "Could not detach the thread");
    }
}
void list_formatter(int i,char buf[]){
    char number[4];
    sprintf(number, "%d",i);
    strcat(buf,number);
    strcat(buf,user_names[i]);
    strcat(buf,"\n");
}