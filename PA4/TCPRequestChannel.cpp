#include "common.h"
#include "TCPRequestChannel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>

using namespace std;


TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    if(_ip_address=="")//if server
	{
        //set variables
		struct addrinfo hints, *server;

        //other servers address
	    struct sockaddr_storage other_addr;  
	    socklen_t sin_size;
	    char buf[INET6_ADDRSTRLEN];
	    int rc;

        //provide necessary machine info for sockaddr_in
        //address family, IPv4
        //IPv4 address, use current IPv4 address (INADDR_ANY)
        //connection port
        //convert short from host bye order to network byte order
	    memset(&hints, 0, sizeof hints);
	    hints.ai_family = AF_UNSPEC;
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_flags = AI_PASSIVE; 

	    if ((rc = getaddrinfo(NULL, _port_no.c_str(), &hints, &server)) != 0) {
	        cerr  << "get address info of other server: " << gai_strerror(rc) << endl;
	        exit(-1);
	    }

        //Normally only a single protocol exists to support a particular socket type
        //within a given protocol family, in which case protocol can be specifies as 0
		if ((sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol)) == -1) {
	        perror("serverer: socket");
			exit(-1);
	    }

        //bind -- assign address to socket - bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
	    if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
			close(sockfd);
			perror("serverer: bind");
			exit(-1);
		}

        //Done
	    freeaddrinfo(server); 

        // listen - listen for client - listen(int sockfd, int backlog)
	    if (listen(sockfd, 20) == -1) {
	        perror("listen");
	        exit(1);
	    }
	    cout << "The serverer is ready to accept connections in port " << _port_no <<endl;

	}
	else //if client side 
	{
        //set variables
		struct addrinfo hints, *client;

        //generate server's info based parameters
        //address family, IPv4
        //connection port
        //convert short from host byte order to network byte order
        //convert ip address c-string to binar representation for sin_addr
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
        int rd;

        //load up address structs with getaddrinfo():
		if ((rd = getaddrinfo (_ip_address.c_str(), _port_no.c_str(), &hints, &client)) != 0) {
	        cerr << "get address info of other server: " << gai_strerror(rd) << endl;
	        exit(-1);//constructor can not return 
	    }

        //socket - make socket(int domain, int type, int protocol)
		sockfd = socket(client->ai_family, client->ai_socktype, client->ai_protocol);
		if (sockfd < 0){
			perror ("Cannot create socket");	
			exit(-1);
		}

		//connect - connect to listening socket - connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
		if (connect(sockfd, client->ai_addr, client->ai_addrlen)<0){
			perror ("Cannot Connect");
			exit(-1);
		}
		
		freeaddrinfo(client);//added

		cout << "Socket is Ready and Connected to the IP" << endl;
	}
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    //assign an existing socket to on=bjects socket file descriptor
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    //close socket - 
    close(this->sockfd);
}

int TCPRequestChannel::accept_conn() {
    // accept connection - need variable to accept the file descriptor
    int client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // socket file descriptor for accepted connection
    // accept connection - accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    // check if sockfd is a valid open socket
    // accept connection
    client_sock = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
    cout << "Inside TCP client_sock is: " << client_sock << endl;
    if (client_sock == -1) {
        perror("Error accepting connection in tcp");
        return -1;
    }

    // return socket file descriptor
    return client_sock;
}


int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    int read = recv(sockfd, msgbuf, msgsize, 0);
    if (read < 0) {
        perror("Client read failed\n");
        //return;
    }
    return read;
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    int write = send(sockfd, msgbuf, msgsize, 0);  
    if (write < 0) {
        perror("Client write failed\n");
        //return;
    }
    return write;
}


