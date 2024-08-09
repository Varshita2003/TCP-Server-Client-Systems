-> Activity 2 – HTTP server 

HOW TO RUN THE CODE : 

1. Run the server

	For example :
	$ gcc -o server severQ2_CS21B036_CS21B056.c -lpthread
	$ ./server 8080 /home/.../webroot
	
	8080 -> Port
	/home.../webroot -> file directory location of where the webpages are stored.
	These 2 parameters can be changed
3. Open a browser and try opening webpages as follows
	http://192.168.0.109:8080/index.html
	192.168.0.109 -> IP address of server
	8080 Port
	index.html -> Requested page
	
DETAILS ABOUT THE CODE :
1. The code using the second argument and the address provided by the client load the webpage required if present or the error page is displayed
2. The code has used a hard coded approach to display the error file that must be changed depending on where the files are stored on the device running the server.
3. The above code was hard coded to make sure the 404 page is displayed even if the root_directory given in the 2nd argument does not have the 404.html file.
4. The post request from the index.html page gives the number of characters, words and sentences displayed on the webpage to the client.
5. The number of words are determined by the space ' ' and the sentences by a full stop '.'
6. A max buffer capacity was set for the POST requests input.
