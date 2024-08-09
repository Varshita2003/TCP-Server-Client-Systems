-> Activity 1 - Music Streaming TCP server

HOW TO RUN CODE:
 1. Setup server by running the server code.
    
    Run these commands on terminal
    $ gcc -o serverQ1_CS21B036_CS21B056 serverQ1_CS21B036_CS21B056.c -lpthread
    $ ./serverQ1_CS21B036_CS21B056 <port> <mp3_dir> <max_streams>
       Replace <port> with some non-special port number, <mp3_dir> with path to directory containing mp3 Songs, <max_streams> with desired number
       
 2. In client code, set the IP address as IP address of server, Port number as number which is specified while running server
    
    Run these commands on terminal
    $ gcc -o client music_tcp_client.c 
    $ ./client
    $ A prompt asking for a number between 1-9 will be displayed, enter any number between 1-9


DETAILS:
1. Server is mutlithreaded program, where each thread handles 1 client.
2. Client IP address will be printed whenever a client request is accepted.
3. When the client enters a song number the corresponding mp3 file is sent to client through socket.
4. There is a shared global variable named active_threads which is updated whenever a client/joins, when active_threads reaches max_streams, any additional clients will be accepted and then rejected  without streaming any song.
5. When client is rejected a message is sent to client that server is busy but client code is interpreting it as mp3 file, so eventually client runs into error and stops.
6. Note that the song names should be 1.mp3, 2.mp3 ..... in the given directory.


    
