-> Activity 3 – TCP chat client 

HOW TO RUN THE CODE : 

1. Make changes to the client code at the port and IP address accordingly.
2. Run the server

	For example :
	$ gcc -o server severQ3_CS21B036_CS21B056.c -lpthread
	$ ./server 8080 4 30 
	
	8080 -> Port
	4    -> Max clients 
	30   -> Timeout seconds
	These 3 parameters can be changed
3. Run the client code as follows
	
	$ gcc -o client client.c
	$ ./client
	
DETAILS ABOUT THE CODE :
1. In the main of the code the connections from the clients are accepted and a client is asked for their username only if the current number of active users is less than the max possible users. 
2. The clients who are given a chance to give their username are disconnected if there is no response within the specified timeout time.
3. After getting the username of the client a thread is created to handle the messages from the client making sure the client is not inactive more that the timeout time.
4. Used locks mainly while collecting the username and when removing a client.
5. A point to note is that only one user at a time can give the username to the server, this is sequential.
6. A user who gives a username which is already used is disconnected from the server.
7. There is a limitation imposed on the number of characters of the messages and username sent and set by user.
