#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"
#include <iostream>
#include <sys/select.h>

/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
//#define _DEBUG
using std::cerr;
using std::endl;
using std::cout;
using std::cin;

int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();
    
	while (1) {
	
		int sockfd = connect_to(argv[1], atoi(argv[2]));
    
		char command[MAX_DATA];

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0 && reply.port !=-1) {
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
		}
	
		close(sockfd);
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

    int masterFd, incommingFD;
    struct addrinfo hints, *result;
    struct sockaddr_storage incommingAddr;
    socklen_t incommingAddrLen = sizeof(incommingAddr);
    char ip[INET6_ADDRSTRLEN], buff[MAX_DATA];
    int returnValue;

	std::string portStr = std::to_string(port);
    
   
    //initalize addrinfo hints 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

#ifdef _DEBUG
	printf("Client attempting to connect to server port:%d\n",port);
#endif //_DEBUG
 
    if( (returnValue = getaddrinfo(host,(char*)portStr.c_str(), &hints, &result)) != 0){
        
        cerr <<"Server Error getaddrinfo: "<<gai_strerror(returnValue) <<endl;
        exit(-1);
    }
    masterFd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if(masterFd == -1){
        
        cerr << "Server Error socket"<<endl;
        exit(-1);
    }

	if(connect(masterFd, result->ai_addr, result->ai_addrlen)<0){
		perror("Server Error cannot connect");
		exit(-1);
	}
	return masterFd;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------

	// REMOVE below code and write your own Reply.

	get_command(command,MAX_DATA);
	send(sockfd,command,MAX_DATA,0);
#ifdef _DEBUG
	printf("Server Command Sent\n");
#endif

	char* recBuff = new char[sizeof(Reply)];
	int iRet =0;
	iRet =recv(sockfd, recBuff,sizeof(Reply),0);

	Reply* pReply = (Reply*)recBuff;
	//process the command

#ifdef _DEBUG
	printf("Server reply recieved\n");
#endif

	struct Reply reply;
	reply.status = pReply->status;
	reply.num_member = pReply->num_member;
	reply.port = pReply->port;

	delete[] recBuff;
	return reply;
}

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------

	int sockfd = connect_to(host,port);

	//set up select to listen to cin and sockfd
	std::string exitStr(">Warnning: the chatting room is going to be closed...\n");
	while(1){
		fd_set rfds;
		struct timeval tv;
		int retval;

		char recvBuff[MAX_DATA];
		char messageBuff[MAX_DATA];

        FD_ZERO(&rfds);
		//add socket
        FD_SET(sockfd,&rfds);
		//add stdin
		FD_SET(0,&rfds);
		retval = select(20, &rfds,NULL,NULL,NULL);
		
		if(retval == -1){
			cerr<<"Chatroom Select"<<endl;
		}

		if(FD_ISSET(sockfd,&rfds)){
			 if((retval= read(sockfd,recvBuff,MAX_DATA))==0){
                        
                //Empty read client has disonnected remove from list
#ifdef _DEBUG
                printf("Chat Mode disconnected:\n");
#endif //_DEBUG
				return;
             }
            else{//Message recieved put into outward message queue 
                    
#ifdef _DEBUG
    	             printf("Chat Mode message recieved length:%d\n",retval);
#endif //_DEBUG
				//chekcing if shutdown command recieved 
				if(recvBuff[retval-1]=='@'){
#ifdef _DEBUG
    	             printf("Chat Mode shut down recieved\n");
#endif //_DEBUG
					display_message((char*)exitStr.c_str());
					return;
				}
				else{//normal message recieved
				
					if(recvBuff[MAX_DATA-1]=='|')	{
							
#ifdef _DEBUG
    	             printf("Chat Mode display_message:\n");
#endif //_DEBUG

	    				printf("> %s", recvBuff);
	    				printf("\n");
						//display_message(recvBuff);
					}
				}
            }
		}

		//check cin for user message and send if any
		if(FD_ISSET(0,&rfds)){

			retval = read(0,messageBuff,MAX_DATA);
			messageBuff[retval-1]='\0';
			send(sockfd,messageBuff,retval,0);
		}
	}
}
#undef _DEBUG

