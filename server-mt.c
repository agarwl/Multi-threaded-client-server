#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <queue>
#include <vector>

#define min(a, b) ((a < b) ? a : b)
#define BUF_SIZE 30

// a function to send buflen bytes to client
int senddata(int sock, void *buf, int buflen);
// a function to send a file to the the client
int sendfile(int sock, FILE *f);

void error(const char *msg)
{
    perror(msg);
    exit(1);
}


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_has_space = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_has_data = PTHREAD_COND_INITIALIZER;
static std::queue<int> pending_requests;


void* handle_request(void *);
void handle_connection(const int & );

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n,num_threads,queue_limit;
     if (argc < 4) {
           fprintf(stderr,"usage %s port numthreads queue-limit\n", argv[0]);   //error if less arguments provided
           exit(0);
     }

     portno = atoi(argv[1]);
     num_threads = atoi(argv[2]);
     queue_limit = atoi(argv[3]);

     /* create socket */

     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");

    // Used so we can re-bind to our port while a previous connection is still in TIME_WAIT state.
    int yes = 1;

    // For re-binding to it without TIME_WAIT problems 
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        error("setsockopt");
    }

     /* fill in port number to listen on. IP address can be anything (INADDR_ANY) */

     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);

     /* bind socket to this port number on this machine */

     if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     /* listen for incoming connection requests */


     std::vector<pthread_t> worker_threads(num_threads);
     for (int i = 0; i < num_threads; ++i)
     {
        if(pthread_create(&worker_threads[i],NULL,handle_request,NULL) < 0)
            error("Thread not created");
     }

     listen(sockfd,100);
     socklen_t clilen = sizeof(cli_addr);
     bool bounded = (queue_limit != 0);

     while(1){

        /* accept a new request, create a newsockfd */
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        // if error on creating new socket, exit
        if (newsockfd < 0) 
            error("Error on accept");

        pthread_mutex_lock(&mutex);
        while(pending_requests.size() >= queue_limit  && bounded )
            pthread_cond_wait(&queue_has_space,&mutex);
        pending_requests.push(newsockfd);
        pthread_cond_broadcast(&queue_has_data);
        pthread_mutex_unlock(&mutex);
    }
    return 0; 
}

void* handle_request(void *x)
{
    int newsockfd;
    while(1){

      pthread_mutex_lock(&mutex);
      
      while(pending_requests.empty())
        pthread_cond_wait(&queue_has_data,&mutex);
      
      newsockfd = pending_requests.front();
      pending_requests.pop();
      pthread_cond_signal(&queue_has_space);
      pthread_mutex_unlock(&mutex);
      handle_connection(newsockfd);
    }
} 

void handle_connection(const int & newsockfd)
{
    char buffer[BUF_SIZE];
    char* filename = NULL;
    bzero(buffer,BUF_SIZE);

    if(read(newsockfd,buffer,BUF_SIZE) < 0)
      error("Error on Reading");

    // the command passed was of form "get filename"
    if(strncmp(buffer,"get",3) == 0){        
        // point filename to the correct pointer
        filename = buffer + 4;              
        printf("File requested: %s\n",filename);
    }

    // send the file to the client
    if(filename != NULL){

        FILE *filehandle = fopen(filename, "rb");
        if (filehandle != NULL){
          sendfile(newsockfd, filehandle);
          fclose(filehandle);
        }
    }
    //close the connect
    close(newsockfd);
}


int senddata(int sock, void *buf, int buflen)
{
    unsigned char *pbuf = (unsigned char *) buf;
    // keep on sending until buflen bytes are not send
    while (buflen > 0)
    {
        int num = send(sock, pbuf, buflen, 0);
        pbuf += num;
        buflen -= num;
    }

    return 1;
}


int sendfile(int sock, FILE *f)
{
    // calculate file size
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    rewind(f);
    if (filesize == EOF)
        return 0;
    // send file using a buffer length of 1024 bytes
    if (filesize > 0)
    {
        char buffer[1024];
        do
        {
            size_t num = min(filesize, sizeof(buffer));
            num = fread(buffer, 1, num, f);
            if (num < 1)
                return 0;
            if (senddata(sock, buffer, num) == 0)
                return 0;
            filesize -= num;
        }
        while (filesize > 0);
    }
    return 1;
}    