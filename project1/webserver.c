/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <time.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

const int BUFLEN = 1024;
int port;

static char* OK_TEMPLATE =
	"HTTP/1.1 200 OK\n"
	"Content-type: %s\n"
	"\n";

 

static char* DEFAULT_TEMPLATE = 
	"HTTP/1.1 200 OK\n"
	"Content-type: text/html\n"
	"\n"
	"<!DOCTYPE html>\n"
	"<html lang=en>\n"
	"  <meta charset=utf-8>\n"
	"  <meta name=viewport content='initial-scale=1, minimum-scale=1, width=device-width'>\n"
	"  <title>Webserver</title>\n"
	"  \n"
	"  <p>This server provides you html, jpg, pdf, and jpeg files.</p>\n"	
	"  <p>You may try to access <a href=\"http://localhost:%d/report.pdf\">report.pdf</a></p>\n"
	"  <p>You may try to access <a href=\"http://localhost:%d/README.txt\">README.txt</a></p>\n"
	"  <p>You may try to access <a href=\"http://localhost:%d/webserver.c\">webserver.c</a></p>\n"
	"  <p>You may try to access <a href=\"http://localhost:%d/image.jpg\">image.jpg</a></p>\n"
	"  <p>You may try to access <a href=\"http://localhost:%d/Makefile\">Makefile</a></p>\n"
	"</body></html>\n";

static char* BAD_REQUEST_TEMPLATE = 
	"HTTP/1.1 400 Bad Request\n"
	"Content-type: text/html\n"
	"\n"
	"<!DOCTYPE html>\n"
	"<html lang=en>\n"
	"  <meta charset=utf-8>\n"
	"  <meta name=viewport content='initial-scale=1, minimum-scale=1, width=device-width'>\n"
	"  <title>Error 400 (Bad Request)!!</title>\n"
	"  <p><b>400.</b> <ins>That’s an error.</ins></p>\n"
	"  <p>This server didn't understand your request.</p></body></html>\n";

static char* NOT_FOUND_TEMPLATE = 
	"HTTP/1.1 404 Not Found\n"
	"Content-type: text/html\n"
	"\n"
	"<!DOCTYPE html>\n"
	"<html lang=en>\n"
	"  <meta charset=utf-8>\n"
	"  <meta name=viewport content='initial-scale=1, minimum-scale=1, width=device-width'>\n"
	"  <title>Error 404 (Not Found)!!</title>\n"
	"  <p><b>404.</b> <ins>That’s an error.</ins></p>\n"
	"  <p>The requested URL <code>%s</code> was not found on this server.  <ins>That’s all we know.</ins></p></body></html>\n";

static char* BAD_METHOD_TEMPLATE = 
	"HTTP/1.1 501 Method Not Implemented\n"
	"Content-type: text/html\n"
	"\n"
	"<!DOCTYPE html>\n"
	"<html lang=en>\n"
	"  <meta charset=utf-8>\n"
	"  <meta name=viewport content='initial-scale=1, minimum-scale=1, width=device-width'>\n"
	"  <title>Error 501 (Not Found)!!</title>\n"
	"  <p><b>501.</b> <ins>That’s an error.</ins></p>\n"
	"  <p>The method <code>%s</code> was not implemented on this server. </p></body></html>\n";

void error(char *msg)
{
	perror(msg);
	exit(1);
}

static int get_file (int requestfd, char* path) {
	char c, *file_type, *content_type;
	int file_fd, i;
	char response[BUFLEN];
	memset(response,0,BUFLEN);

	printf("Retrieving resource %s\n", path);

	//Return greeting page upon first request.
	if (strcmp(path,"/") == 0)
	{
		snprintf(response, BUFLEN-1, DEFAULT_TEMPLATE, port, port, port, port, port);
		write(requestfd, response, strlen(response));
		return 0;
	}

	//Return 404 error if file is not found
	file_fd = open(++path, O_RDONLY);
	if (file_fd < 0) {
		snprintf (response, BUFLEN-1, NOT_FOUND_TEMPLATE, path);
		write (requestfd, response, strlen (response));
		printf ("ERROR opening file\n");
		return -1;
	}

	// Get the file extension. If there isn't one explicitly stated, then assume HTML
	file_type = strchr (path, '.');
	if (!file_type)
		file_type = "text/html";
	else
		file_type++;

	if (strcasecmp (file_type, "jpeg") == 0)
		content_type = "image/jpeg";
	else if (strcasecmp (file_type, "jpg") == 0)
		content_type = "image/jpeg";
	else if (strcasecmp (file_type, "ico") == 0)
		content_type = "image/x-ico";
	else if (strcasecmp (file_type, "pdf") == 0)
		content_type = "application/pdf";
	else
		content_type = "text/plain";

	// Begin to write the response to the client: OK, file type
	snprintf(response, BUFLEN-1, OK_TEMPLATE, content_type);
	write(requestfd, response, strlen (response));
    
	while ( (i = read(file_fd, &c, 1)) ) {
		if ( write(requestfd, &c, 1) < 0 )
			error("ERROR writing to socket");
	}
	close(file_fd);

	return 0;
}

int processRequest(char* buffer, int sockfd)
{
	char method[BUFLEN];
	char path[BUFLEN];
	char protocol[BUFLEN];
	char response[BUFLEN]; //response message
	memset(response, 0, BUFLEN);

	/*If the header does not contain three fields for HTTP protocol, we don’t understand it.*/
    int n;
	n = sscanf(buffer, "%s %s %s",method, path, protocol);
	if (n == EOF || n < 3){
		write (sockfd, BAD_REQUEST_TEMPLATE, strlen(BAD_REQUEST_TEMPLATE));
		return -1;
	}

	// For debugging purposes
	// printf("Here are Headers:%s %s %s\n",method, path, protocol);

	/*Check the validity of request*/
	/*method field: cannot process any method other than GET*/
	if (strcmp(method, "GET")){
		snprintf (response, BUFLEN-1, BAD_METHOD_TEMPLATE, method);
		write (sockfd, response, strlen(response));
		return -1;
	}
	/*protocol field: cannot process any HTTP protocol other than HTTP 1.0 or 1.1*/
	else if (strcasecmp(protocol, "HTTP/1.0") && strcasecmp(protocol, "HTTP/1.1")) {
		write (sockfd, BAD_REQUEST_TEMPLATE, strlen(BAD_REQUEST_TEMPLATE));
		return -1;
	}
	else
		return get_file (sockfd, path);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	if (argc < 2) {
	 fprintf(stderr,"ERROR, no port provided\n");
	 exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	//create socket
	if (sockfd < 0) 
	error("ERROR opening socket");
	memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
	//fill in address info
	portno = atoi(argv[1]);
	port = portno;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
	      sizeof(serv_addr)) < 0) 
	error("ERROR on binding");

	while(1){
		listen(sockfd,5);	//5 simultaneous connection at most

		//accept connections
	 	newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	 
		if (newsockfd < 0) 
			error("ERROR on accept");

		int n;
		char buffer[BUFLEN];

		memset(buffer, 0, BUFLEN);	//reset memory

		//read client's message
		n = recv(newsockfd,buffer,BUFLEN-1,0);
		if (n < 0)
			error("ERROR reading from socket");
		printf("Here is the message: \n%s\n",buffer);
		/*Null terminate the buffer so we can use string operations on it.*/
		buffer[n] = '\0'; 

		/*process request*/
		n = processRequest(buffer, newsockfd);
		/*process failed*/
		if (n == -1) 
			printf("Request was not successful.");

		printf("\n---------------------next request--------------------\n");
		close(newsockfd);//close connection 
	}
	 
	close(sockfd);
	return 0; 
}



