#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>
#include <string.h>

#define min(a, b) ((a < b) ? a : b)
#define NUM_FILES 10000
#define BUF_SIZE 30

void *connection(void *);       //establish connection with server and request files
int read_and_discard(int sock); //read file and discard
struct hostent *server;
int portno,runtime,sleeptime;
char *mode;

int *num_requests;
double *response_time;

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, n;

    struct sockaddr_in serv_addr;

   
    if (argc < 7) {
       fprintf(stderr,"usage %s hostname port users time sleep mode\n", argv[0]);   //error if less arguments provided
       exit(0);
    }

    portno = atoi(argv[2]);             //set portno as second input argument
    server = gethostbyname(argv[1]);    //set host as first input argument
    int num_client = atoi(argv[3]);     //set thread count as third input argument
    runtime = atoi(argv[4]);            //set runtime as fourh input argument
    sleeptime = atoi(argv[5]);          //set sleep between downloads as fifth input argument
    mode = argv[6];                     //set file download mode as sixth input argument

    num_requests = malloc(num_client * sizeof(int));        //number of requests of each thread
    response_time =  malloc(num_client * sizeof(double));   //response time of each thread

    pthread_t tid[num_client];          //array of threads 
    int i;
    for (i=0; i<num_client; i++) //create threads by calling function 'connection' and passing thread number as argument
    {   
        if( pthread_create( &tid[i] , NULL ,  connection , (void*) i) < 0)      
        {
            error("could not create thread");
        }
    }
    for (i = 0; i < num_client; i++)
       pthread_join(tid[i], NULL);      //join the threads when all have been executed

   double total_req=0,throughput,avg_response_time=0;
   for ( i = 0; i < num_client; ++i)
   {
        total_req += num_requests[i];         //total reequests is sum of all individual thread requests
        avg_response_time += response_time[i];
       /* code */
   }
   throughput = total_req/runtime;            //throughput is number of total requests by runtime
   avg_response_time /= total_req;            //average response time is sum of all individual response times by num of requests

   printf("throughput = %f req/s\n",throughput);        //print throughput
   printf("average response time = %f sec\n",avg_response_time);        //print average response time

   free(num_requests);                         //free the dynamically allocated mmory
   free(response_time);

   return 0;

}

void *connection(void *threadid)
{
    int threadnum = (int)threadid;
    int sockfd;                   // socket file descriptor
    struct sockaddr_in serv_addr; //server address info
    int n;

    char filename[BUF_SIZE];
    char temp[5];
    int filenum;                  // read the files/foo0.txt file by default if the mode is fixed


    num_requests[threadnum] = 0;
    response_time[threadnum] = 0;
    srand(threadnum);             //provides seed to the random generator         

    if (server == NULL) {       //check if host exists
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof (serv_addr));

    serv_addr.sin_family = AF_INET;     //server host byte order
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length); //server address   
    serv_addr.sin_port = htons(portno); //server port network byte order   

    time_t start= time(NULL);
    time_t now;

    int printed = 0;
    while (1)
    {
        now = time(NULL);           //start timer
        if ( now - start > runtime)
            break;

        sockfd = socket(AF_INET, SOCK_STREAM, 0); //create socket
        if (sockfd < 0){                             //check if successfully created
            error("ERROR opening socket");
        }

        if (connect(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){ //connect to socket and check
            perror("ERROR connecting to server");
            pthread_exit(NULL);
            // continue;
        }

        if( !printed) {
            printf("accepted new connection %d\n", sockfd);
            printed=1;
        }

        if(strcmp(mode,"random") == 0)      //if random mode, generate random requests
            filenum = rand() % NUM_FILES;
        else if(strcmp(mode,"fixed") == 0)  //else request fixed file (foo0.txt)
            filenum = 0;
       
        //generate file name as required
        bzero(filename,BUF_SIZE); 
        strcpy(filename, "get files/foo");
        sprintf(temp,"%d",filenum);
        strcat(filename,temp);
        strcat(filename,".txt");

        n = write(sockfd,filename,strlen(filename)); //request server for files
        if (n < 0) {
            perror("ERROR writing to socket");
            continue;
        }
        now = time(NULL);

        int ok = read_and_discard(sockfd); //read the file 1024 bytes at a time and discard them
        if (ok == 1){
            response_time[threadnum] += (time(NULL) - now); //update response time
            num_requests[threadnum]++;                      //update number of requests
            printf("File received: %s\n", filename+10);
        }
        else
        {
            perror("File send fail");
            continue;
        }

        close(sockfd);  //close socket
        sleep(sleeptime); //sleep after download
    }
   
    return 0;
}

int read_and_discard(int sock) 
{
    char buffer[1024];  //read 1024 bytes at a time
    size_t bufflen = sizeof(buffer); //size of buffer
    int bytes_read;
    while(( bytes_read = read(sock, buffer, bufflen) ) > 0); //read from socket into buffer

    if(bytes_read == 0)
        return 1;
    else
        return 0;
}