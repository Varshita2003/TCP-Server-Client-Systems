#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PAGE "index.html"
#define NOT_FOUND_PAGE "404.html"

struct ThreadArgs 
{
    int client_socket;
    const char *root_directory;
};

void *client_thread(void *arg);
void send_file(int client_socket, const char *file_path);
void handle_get_request(int client_socket, const char *request, const char *root_directory);
void handle_post_request(int client_socket, const char *request);

int main(int argc, char *argv[]) 
{
    if (argc != 3) 
    {
        fprintf(stderr, "Usage: %s <port> <root_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, '\0', sizeof(server_addr)); // zero structure out
    server_addr.sin_family = AF_INET; // match the socket() call
    server_addr.sin_port = htons(atoi(argv[1])); // specify port to listen on
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // bind to any local address

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Bind error");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) 
    {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %s\n\n\n", argv[1]);

    while (1) 
    {
        // Accept connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) 
        {
            perror("Accept error");
            continue;
        }

        // Create a new thread to handle the client
        struct ThreadArgs *thread_args = malloc(sizeof(struct ThreadArgs));
        if (thread_args == NULL) 
        {
            perror("Memory allocation error");
            close(client_socket);
            continue;
        }

        thread_args->client_socket = client_socket;
        thread_args->root_directory = argv[2];
        
        if (pthread_create(&tid, NULL, client_thread, thread_args) != 0) 
        {
            perror("Thread creation error");
            close(client_socket);
            free(thread_args);
            continue;
        }
    }

    close(server_socket);
    return 0;
}

void *client_thread(void *arg) 
{
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int client_socket = args->client_socket;
    const char *root_directory = args->root_directory;

    free(arg);  // Free the thread argument structure

    char request[BUFFER_SIZE];
    memset(request, 0, sizeof(request));

    // Receive client request
    if (recv(client_socket, request, sizeof(request), 0) == -1) 
    {
        perror("Receive error");
        close(client_socket);
        pthread_exit(NULL);
    }

    // printf("Request: %s\n", request);
    //______________________________________

    char method[10];
    sscanf(request, "%s", method);

    if (strcmp(method, "GET") == 0) 
    {
        handle_get_request(client_socket, request, root_directory);
    } 
    else if (strcmp(method, "POST") == 0) 
    {
        handle_post_request(client_socket, request);
    } 
    else 
    {
        // Unsupported method
        close(client_socket);
    }

    pthread_exit(NULL);
}

void handle_get_request(int client_socket, const char *request, const char *root_directory) 
{
    char file_path[BUFFER_SIZE];
    sscanf(request, "GET %s", file_path);
    char temp[BUFFER_SIZE];
    strcpy(temp,file_path);

    if (strcmp(file_path, "/") == 0) 
    {
        // If no webpage is mentioned, serve the index.html by default
        snprintf(file_path, sizeof(file_path), "%s/%s", root_directory, DEFAULT_PAGE);
    } 
    else 
    {
        snprintf(file_path, sizeof(file_path), "%s%s", root_directory, temp);
    }

    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) 
    {
        // File not found, serve 404.html
        // snprintf(file_path, sizeof(file_path), "%s/%s", root_directory, NOT_FOUND_PAGE); 
        // the above code line shall be used if we only want to restrict the occurance of the default page when in the webroot directory

        //the below line will displat 404.html irrespective of the error occuring in the case for any root directory given as the argument by the user
        //need to set the address for the path where the 404.html page is present as below

        snprintf(file_path, sizeof(file_path), "%s/%s", "/home/harshitha/Documents/6th_sem/CS3205/Assignment/A2/Q2/webroot", NOT_FOUND_PAGE); 

    }

    send_file(client_socket, file_path);
}

void handle_post_request(int client_socket, const char *request) 
{
    char *content_length_header = strstr(request, "Content-Length:");
    if (content_length_header == NULL) {
        // Invalid POST request without Content-Length header
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }

    // Extract content length from the header
    size_t content_length;
    sscanf(content_length_header, "Content-Length: %zu", &content_length);

    char *post_data = malloc(content_length + 1);  // Allocate memory for post data (+1 for null terminator)
    if (post_data == NULL) {
        // Memory allocation error
        const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }

    // Read the post data from the request
    char *content_start = strstr(request, "\r\n\r\n") + 4;  // Find start of post data
    strncpy(post_data, content_start, content_length);
    post_data[content_length] = '\0';  // Null-terminate the string

    // Find the start of the message data
    char *message_start = strstr(post_data, "name=\"message\"");
    if (message_start == NULL) {
        // "message" parameter not found
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        free(post_data);
        return;
    }
    message_start = strstr(message_start, "\r\n\r\n") + 4;  // Move past headers to actual message data

    // Find the end of the message data
    char *message_end = strstr(message_start, "\r\n");
    if (message_end == NULL) {
        // Invalid message data format
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        free(post_data);
        return;
    }

    // Extract the message
    *message_end = '\0';  // Null-terminate to treat as a string
    char *message = message_start;

    // Remove the leading and trailing %**% from the message
    char *stripped_message = message + 4;  // Move past the leading %**%
    message_end -= 4;  // Adjust the end pointer to exclude the trailing %**%
    *message_end = '\0';  // Null-terminate the stripped message

    // Process the message (count characters, words, sentences)
    int num_characters = strlen(stripped_message);
    int num_words = 0;
    int num_sentences = 0;
    char *token = strtok(stripped_message, " ");  // Tokenize by space to count words
    while (token != NULL) {
        num_words++;
        token = strtok(NULL, " ");
    }
    // Count sentences by looking for '.', '!', '?'
    for (int i = 0; i < num_characters; i++) {
        if (stripped_message[i] == '.' || stripped_message[i] == '!' || stripped_message[i] == '?') {
            num_sentences++;
        }
    }

    // Prepare response with the count information
    char response_buffer[256];  // Assuming response fits in this buffer
    snprintf(response_buffer, sizeof(response_buffer), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                                        "<html><body>"
                                                        "<h1>Post Request Processed</h1>"
                                                        "<p>Number of characters: %d</p>"
                                                        "<p>Number of words: %d</p>"
                                                        "<p>Number of sentences: %d</p>"
                                                        "</body></html>",
             num_characters, num_words, num_sentences);

    send(client_socket, response_buffer, strlen(response_buffer), 0);
    close(client_socket);

    free(post_data);
}

void send_file(int client_socket, const char *file_path) 
{
    FILE *file = fopen(file_path, "rb");
    if (!file) 
    {
        perror("File open error");
        close(client_socket);
        return;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_buffer = (char *)malloc(file_size);
    if (!file_buffer) 
    {
        perror("Memory allocation error");
        fclose(file);
        close(client_socket);
        return;
    }

    fread(file_buffer, 1, file_size, file);
    fclose(file);

    const char *http_header = "HTTP/1.1 200 OK\r\nContent-Type: ";
    const char *content_type = strrchr(file_path, '.');
    const char *default_content_type = "application/octet-stream";

    if (content_type) 
    {
        content_type++;
        if (strcmp(content_type, "html") == 0) 
        {
            default_content_type = "text/html";
        } 
        else if (strcmp(content_type, "css") == 0) 
        {
            default_content_type = "text/css";
        } 
        else if (strcmp(content_type, "js") == 0) 
        {
            default_content_type = "application/javascript";
        } 
        else if (strcmp(content_type, "jpg") == 0 || strcmp(content_type, "jpeg") == 0) 
        {
            default_content_type = "image/jpeg";
        } 
        else if (strcmp(content_type, "png") == 0) 
        {
            default_content_type = "image/png";
        }
    }

    char response_header[BUFFER_SIZE];
    snprintf(response_header, sizeof(response_header), "%s%s\r\nContent-Length: %zu\r\n\r\n", http_header, default_content_type, file_size);

    // Send HTTP header
    send(client_socket, response_header, strlen(response_header), 0);

    // Send file content
    send(client_socket, file_buffer, file_size, 0);

    free(file_buffer);
    close(client_socket);
}