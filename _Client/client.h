
// macros for handling errors
#define handle_error_en(en, msg)    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg)           do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* Configuration parameters */
#define DEBUG           1   // display debug messages
#define MAX_CONN_QUEUE  3   // max number of connections the server can queue
#define AWS             1
#if AWS
    #define SERVER_ADDRESS  "35.180.35.239" //AWS
#else
    #define SERVER_ADDRESS  "127.0.0.1" //AWS
#endif
#define SERVER_COMMAND  "QUIT\n"
#define SERVER_PORT     2015
#define ERROR_MSG       "0xAFFAF\n"
#define ERROR_OFF       0xDEAD
#define OK_MSG          "OK\n"
#define ALONE_MSG       "Alone\n" 
#define NAME            1

//Struct define
typedef struct handler_args_m
{
    int socket_desc;
} handler_args_m;
struct mesg_buffer { 
    long mesg_type; 
    char mesg_text[100]; 
} message;
typedef struct input_m { 
    long mesg_type; 
    char mesg_text[100]; 
} input_m;
typedef struct handler_args_u{
    GtkWidget * view;
}handler_args_u;
struct user_par{
    GtkWidget* username;
    GtkWidget* password;
    GtkWidget* dialog;
    GtkWidget* window;
} user_par;


//client thread
void* client(void *arg);
//manages incoming messages
void* thread_reciver(void *arg);
//gtk thread
static void activate (GtkApplication *app,gpointer user_data);
GtkWidget* activate_login(GtkApplication *app);
//login checker 
void login( GtkWidget *widget,gpointer data );
//update gtk thread
void* update (void* arg);

//close db connection
static void exit_nicely(PGconn *conn, PGresult   *res);