#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <poll.h>

#define BUFFER_SIZE 1024

struct ThreadArgs 
{
    int client_socket;
    char username[BUFFER_SIZE];
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety
int *client_sockets;
char (*client_usernames)[BUFFER_SIZE];
int active_clients = 0;
int max_clients;
int timeout_seconds; // Timeout duration in seconds

void *handle_client(void *arg);
void send_message_all(char *message, int current_client);
void remove_client(int index);

int main(int argc, char *argv[]) 
{
    // Check command-line arguments
    if (argc != 4) 
    {
        printf("Usage: %s <port_number> <max_clients> <timeout_seconds>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command-line arguments
    int port = atoi(argv[1]);
    max_clients = atoi(argv[2]);
    timeout_seconds = atoi(argv[3]);

    // Allocate memory for client data
    client_sockets = malloc(max_clients * sizeof(int));
    client_usernames = malloc(max_clients * sizeof(char[BUFFER_SIZE]));

    // Initialize server socket and settings
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t client_thread;

    // Create socket
    
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    // Set server address settings
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to the address and port
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Error binding socket");
        return EXIT_FAILURE;
    }

    // Listen for incoming connections
    if (listen(server_sock, max_clients) < 0) 
    {
        perror("Error listening");
        return EXIT_FAILURE;
    }

    printf("Server started. Waiting for connections...\n");

    pthread_mutex_t username_mutex = PTHREAD_MUTEX_INITIALIZER;
    // Accept incoming connections and handle them in separate threads
    while (1) 
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) 
        {
            perror("Error accepting connection");
            continue;
        }

        pthread_mutex_lock(&username_mutex);
        
        char username[BUFFER_SIZE] = {0};
        char message[BUFFER_SIZE];

        printf("A client has connected waiting for username ...\n");

        if (active_clients >= max_clients) 
        {
            printf("Maximum clients reached. Did not accept a new connection.\n");
            // char maxClientError[BUFFER_SIZE];
            // snprintf(maxClientError, BUFFER_SIZE, "Maximum clients reached. Cannot accept new connections.\n");
            // send(client_sock, maxClientError, strlen(maxClientError), 0);
            close(client_sock);
            pthread_mutex_unlock(&username_mutex);
            continue;
        }

        int name_taken = 0;

        char askForUsername[BUFFER_SIZE];
        snprintf(askForUsername, BUFFER_SIZE, "Enter Username : ");
        send(client_sock, askForUsername, strlen(askForUsername), 0);

        struct pollfd fds[1];
        fds[0].fd = client_sock;
        fds[0].events = POLLIN; // Check for incoming data

        int ret = poll(fds, 1, timeout_seconds * 1000); // Timeout in milliseconds

        if (ret == -1)
        {
            perror("Poll error");
            continue;
        }
        else if (ret == 0) // Timeout occurred, no data received
        {
            printf("Client inactive. Closing connection...\n");
            close(client_sock);
            pthread_mutex_unlock(&username_mutex);
            continue;
        }
        else
        {
            if (fds[0].revents & POLLIN)
            {
                int bytes_received = recv(client_sock, username, BUFFER_SIZE, 0);
                if (bytes_received == -1) 
                {
                    printf("Error receiving username from client\n");
                    close(client_sock);
                    pthread_mutex_unlock(&username_mutex);
                    continue;
                } 
                else if (bytes_received == 0) 
                {
                    printf("Client disconnected before sending username\n");
                    close(client_sock);
                    pthread_mutex_unlock(&username_mutex);
                    continue;
                }

                // Null-terminate the username string
                username[bytes_received] = '\0';

                pthread_mutex_lock(&mutex);
                // Check if username already exists
                for (int i = 0; i < active_clients; i++) 
                {
                    if (strcmp(username, client_usernames[i]) == 0) 
                    {
                        char message[BUFFER_SIZE];
                        snprintf(message, BUFFER_SIZE, "Server: Username already exists. Please choose a different username.\n");
                        send(client_sock, message, strlen(message), 0);
                        close(client_sock);
                        pthread_mutex_unlock(&username_mutex);
                        continue;
                    }
                }

                // Add client to active list
                strcpy(client_usernames[active_clients], username);
                client_sockets[active_clients++] = client_sock;
                pthread_mutex_unlock(&mutex);
            }
        }
        

        // Send welcome message and list of existing users
        // snprintf(message, BUFFER_SIZE, "Server: Welcome, %s!\n", username);
        snprintf(message, BUFFER_SIZE, "Server: Welcome, %.*s!\n", (int)strlen(username), username);

        send(client_sock, message, strlen(message), 0);
        printf("%s has joined the chat\n",username);

        snprintf(message, BUFFER_SIZE, "Server: Currently connected users:\n");
        for (int i = 0; i < active_clients; i++) 
        {
            strcat(message,"-> ");
            strcat(message, client_usernames[i]);
            strcat(message, "\n");
        }
        send(client_sock, message, strlen(message), 0);

        // Notify all clients about the new connection
        // snprintf(message, BUFFER_SIZE, "Server: Client %s joined the chatroom.\n", username);
        snprintf(message, BUFFER_SIZE, "Server: Client %.*s joined the chatroom.\n", (int)strlen(username), username);

        send_message_all(message, active_clients - 1);

        pthread_mutex_unlock(&username_mutex);

        // Create a new thread to handle the client
        struct ThreadArgs *thread_args = malloc(sizeof(struct ThreadArgs));
        if (thread_args == NULL) 
        {
            perror("Memory allocation error");
            close(client_sock);
            continue;
        }

        thread_args->client_socket = client_sock;
        strcpy(thread_args->username,username);

        if (pthread_create(&client_thread, NULL, handle_client, thread_args) != 0) 
        {
            perror("Error creating client thread");
            close(client_sock);
            continue;
        }

    }

    close(server_sock);
    free(client_sockets);
    free(client_usernames);
    return EXIT_SUCCESS;
}

void *handle_client(void *arg) 
{
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int client_sock = args->client_socket;
    char username[BUFFER_SIZE];
    strcpy(username,args->username);
    char message[BUFFER_SIZE];

    // Handle client messages
    while (1) 
    {
        // Send "ME: " prompt to the client
        snprintf(message, BUFFER_SIZE, "ME: ");
        send(client_sock, message, strlen(message), 0);

        struct pollfd fds[1];
        fds[0].fd = client_sock;
        fds[0].events = POLLIN; // Check for incoming data

        int ret = poll(fds, 1, timeout_seconds * 1000); // Timeout in milliseconds

        if (ret == -1)
        {
            perror("Poll error");
            int remove_index = -1;
            for (int i = 0; i < active_clients; i++) 
            {
                if (strcmp(username, client_usernames[i]) == 0) 
                {
                    remove_index = i;
                    break;
                }
            }
            remove_client(remove_index);
            break;
        }
        else if (ret == 0) // Timeout occurred, no data received
        {
            printf("%s Client inactive. Closing connection...\n",username);
            int remove_index = -1;
            for (int i = 0; i < active_clients; i++) 
            {
                if (strcmp(username, client_usernames[i]) == 0) 
                {
                    remove_index = i;
                    break;
                }
            }
            remove_client(remove_index);
            break; // Exit the loop and close the client connection
        }

        // If there is data to read
        if (fds[0].revents & POLLIN)
        {
            
            // Receive client's message and process it
            char buffer[BUFFER_SIZE] = {0};
            int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received == -1) 
            {
                int remove_index = -1;
                for (int i = 0; i < active_clients; i++) 
                {
                    if (strcmp(username, client_usernames[i]) == 0) 
                    {
                        remove_index = i;
                        break;
                    }
                }
                
                // snprintf(message, BUFFER_SIZE, "Server: Client %s left the chatroom.\n", username);
                snprintf(message, BUFFER_SIZE, "Server: Client %.*s left the chatroom.\n", (int)strlen(username), username);
                send_message_all(message, active_clients - 1);
                printf("%s has left the chatroom",username);
                remove_client(remove_index);
                break;
            }

            // Process client's message based on commands
            if (strcmp(buffer, "\\list") == 0) 
            {
                // Send list of connected users
                char list_message[BUFFER_SIZE] = {0};
                snprintf(list_message, BUFFER_SIZE, "Server: Currently connected users:\n");
                for (int i = 0; i < active_clients; i++) 
                {
                    strcat(list_message, client_usernames[i]);
                    strcat(list_message, "\n");
                }
                send(client_sock, list_message, strlen(list_message), 0);
            } 
            else if (strcmp(buffer, "\\bye") == 0) 
            {
                // Find the index of the client to be removed based on its username
                int remove_index = -1;
                for (int i = 0; i < active_clients; i++) 
                {
                    if (strcmp(username, client_usernames[i]) == 0) 
                    {
                        remove_index = i;
                        break;
                    }
                }

                if (remove_index != -1) 
                {
                    // Notify all clients about the client leaving the chatroom
                    // snprintf(message, BUFFER_SIZE, "Server: Client %s left the chatroom.\n", username);
                    printf("%s has left the chatroom",username);
                    snprintf(message, BUFFER_SIZE, "Server: Client %.*s left the chatroom.\n", (int)strlen(username), username);
                    send_message_all(message, remove_index);
                    

                    // Send a goodbye message to the client being removed
                    // snprintf(message, BUFFER_SIZE, "Server: Goodbye, %s.\n", username);
                    // snprintf(message, BUFFER_SIZE, "Server: Goodbye, %.*s.\n", (int)strlen(username), username);
                    // send(client_sockets[remove_index], message, strlen(message), 0);

                    // Remove the client from the active list
                    remove_client(remove_index);
                }
                break;
            } 
            else 
            {
                // Send message to all clients except the sender
                int my_index = -1;
                for (int i = 0; i < active_clients; i++) 
                {
                    if (strcmp(username, client_usernames[i]) == 0) 
                    {
                        my_index = i;
                        break;
                    }
                }

              

                // snprintf(message, BUFFER_SIZE, "%s: %s", username, buffer);
                snprintf(message, BUFFER_SIZE, "%.*s: %.*s", (int)strlen(username), username, (int)strlen(buffer), buffer);
                send_message_all(message, my_index);
            }
        }
    }
    close(client_sock);
    pthread_exit(NULL);
}

void send_message_all(char *message, int current_client) 
{
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < active_clients; i++) 
    {
        if (i != current_client) 
        {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

void remove_client(int index) 
{
    pthread_mutex_lock(&mutex);
    close(client_sockets[index]);
    for (int i = index; i < active_clients - 1; i++) 
    {
        client_sockets[i] = client_sockets[i + 1];
        strcpy(client_usernames[i], client_usernames[i + 1]);
    }
    active_clients--;
    pthread_mutex_unlock(&mutex);
}
