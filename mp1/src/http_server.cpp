/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <istream>
#include <time.h>

using namespace std;


#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 4096 // max number of bytes we can get at once 

void sigchld_handler(int s)
{	
    s++;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int numbytes;
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;




	if (argc != 2) {
	    fprintf(stderr,"usage: server port\n");
	    exit(1);
	}



	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP


	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	char buf[MAXDATASIZE];
	FILE *fp;
	int SIZE = 1048575; // 1MB
	char data_in_memory[SIZE];
	for (int i = 0; i < SIZE; i++) {
        data_in_memory[i] = 'a'; // Example filler data
    }



	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		// if (!fork()) { // this is the child process
		close(sockfd); // child doesn't need the listener

		long total_bytes = 0;

		while (true){

			int recv_value = recv(new_fd, buf, MAXDATASIZE, 0);
			int offset = 0;
			if (recv_value == 0) break;

			string new_buf(buf);

			// cout << new_buf << endl;
			if (new_buf.find_first_of("GET ") == 0) {
				new_buf = new_buf.substr(4);
				// cout << new_buf << endl;
				if (new_buf.find(" HTTP") != new_buf.npos) {
					string path = new_buf.substr(1, new_buf.find(" HTTP")-1);
					// cout << path << endl;
					fp = fopen(path.data(), "rb");
					if (fp != NULL) new_buf = "HTTP/1.1 200 OK\r\n\r\n";
					else new_buf = "HTTP/1.1 404 Not Found\r\n\r\n";
				}
				else new_buf = "HTTP/1.1 400 Bad Request\r\n\r\n";
			}
			else new_buf = "HTTP/1.1 400 Bad Request\r\n\r\n";

			if (send(new_fd, new_buf.data(), new_buf.size(), 0) == -1) {
				perror("send");
				exit(1);
			}		

				

			memset(buf, '\0', MAXDATASIZE);

			// gettimeofday(&current_time, NULL);
			// microseconds = (long long)current_time.tv_sec * 1000000 + current_time.tv_usec;
			// printf("%lld\n", microseconds);
			
			// while (true) {
				
			// 	numbytes = fread(buf, sizeof(char), MAXDATASIZE, fp);
			// 	if (numbytes > 0) {
			// 		if (send(new_fd, buf, numbytes, 0) == -1) {
			// 			perror("send");
			// 			exit(1);
			// 		}	
			// 		memset(buf, '\0', MAXDATASIZE);
			// 		total_bytes += numbytes;
			// 	}
			// 	else break;
			// }

			while (offset < SIZE) {
			// Calculate the number of bytes to send in this chunk
			int bytes_to_send = (SIZE - offset > MAXDATASIZE) ? MAXDATASIZE : (SIZE - offset);
			
			// Send the data in chunks of MAXDATASIZE
			numbytes = send(new_fd, data_in_memory + offset, bytes_to_send, 0);
			if (numbytes == -1) {
				perror("send");
				exit(1);
			}

			
			offset += numbytes; // Move the offset forward
			total_bytes += numbytes; // Accumulate the total bytes sent
		}

			fclose(fp);
		}


		// if (send(new_fd, "Hello, world!", 13, 0) == -1)
		// 	perror("send");
		close(new_fd);
		printf("total bytes: %ld", total_bytes);
		// 	exit(0);
		// }
		// close(new_fd);  // parent doesn't need this
		break;
	}

	return 0;
}

