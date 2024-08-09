#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <dirent.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
int active_threads;
char* mp3_dir ;
struct ThreadArgs {
    int song_number;
    int client_soc_no;   
};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


void* send_mp3_file(void* args) {

    printf("thread created \n");
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int client_socket = threadArgs->client_soc_no;
    
    char request[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, request, sizeof(request), 0);
    if (bytes_received <= 0) {
        perror("Error receiving request");
        pthread_mutex_unlock(&lock);
        close(client_socket);
    }

    
    int song_number;
    if (sscanf(request, "%d", &song_number) == 1) 
    {   
        ;  
    } 
    else 
    {
        printf("Invalid command format\n"); 
        pthread_mutex_lock(&lock);
        close(client_socket);
        active_threads -= 1;
        printf("%d -activethreads\n",active_threads);
        pthread_mutex_unlock(&lock);
        pthread_exit(NULL);
    }
    char str[20]; 
    sprintf(str, "%d", song_number); 
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/%s.mp3", mp3_dir, str);
    printf("Streaming: %s\n", filename);

    FILE *mp3_file = fopen(filename, "rb");

    fseek(mp3_file, 0, SEEK_END);
    long file_size = ftell(mp3_file);
    rewind(mp3_file);

    char *file_buffer = (char *)malloc(file_size);
    if (file_buffer == NULL) 
    {
        perror("Memory allocation failed");
        pthread_mutex_lock(&lock);
        close(client_socket);
        active_threads -= 1;
        printf("%d - activethreads\n",active_threads);
        pthread_mutex_unlock(&lock);
        pthread_exit(NULL);
    }

    // Read the entire file into memory
    size_t bytes_read = fread(file_buffer, 1, file_size, mp3_file);
    if (bytes_read != file_size) {
        perror("Error reading file");
        pthread_mutex_lock(&lock);
        close(client_socket);
        active_threads -= 1;
        printf("%d - activethreads\n",active_threads);
        pthread_mutex_unlock(&lock);
        pthread_exit(NULL);
       
    }

    // Send the entire file over the socket
    if (send(client_socket, file_buffer, file_size, 0) != file_size) {
        perror("Error sending data");
        pthread_mutex_lock(&lock);
        close(client_socket);
        active_threads -= 1;
        printf("%d - activethreads\n",active_threads);
        pthread_mutex_unlock(&lock);
        pthread_exit(NULL);
    }


    pthread_mutex_lock(&lock);
    close(client_socket);
    active_threads -= 1;
    printf("%d - activethreads\n",active_threads);
    pthread_mutex_unlock(&lock);

// Free dynamically allocated memory
    free(file_buffer);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <mp3_dir> <max_streams>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    mp3_dir = argv[2];
    int max_streams = atoi(argv[3]);
    active_threads  = 0;

    struct sockaddr_in server_addr, client_addr;
    int server_socket, client_socket;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, max_streams) < 0) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    while (1) {
        // Accept incoming connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }
        pthread_mutex_lock(&lock);
        active_threads += 1;
        printf("%d -activethreads\n",active_threads);
        pthread_mutex_unlock(&lock);

        if(active_threads <= max_streams)
        {
            
            printf("Client connected\n");
            char client_ip[INET_ADDRSTRLEN];
	        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN); 
	        printf("Client IP address: %s\n", client_ip);
            
            struct ThreadArgs *args = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
            args->song_number = 9;
            args->client_soc_no = client_socket;
            pthread_t streaming_thread;
            pthread_create(&streaming_thread,NULL,send_mp3_file, (void*)args);

        }

        else
        {
            // char* message = "Server Busy! Try connecting later";
            printf("client rejected, maximum streaming limit reached\n");

            pthread_mutex_lock(&lock);
            active_threads -= 1;
            printf("%d -activethreads\n",active_threads);
            pthread_mutex_unlock(&lock);
            close(client_socket);
        }
       

    }

    // Close server socket
    close(server_socket);

    return 0;
}
