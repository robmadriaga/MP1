#include "interface.h"
#include <thread>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <pthread.h>
#include <condition_variable>
#include <unordered_map>
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>

//#define _DEBUG
/*using std::vector,
    std::endl,
    std::cout,
    std::semaphore,
    std::mutex,
    std::thread,
    std::unorderd_map,
    std::string,
    std::atomic;
    */
using namespace std; 

class semaphore{
    private:
    int cnt;
    mutex m;
    condition_variable cv;
    public:
    semaphore(int _cnt):cnt(_cnt){

    }

    void acquire(){
        unique_lock<mutex> l (m);
        cv.wait(l, [this]{return cnt >0;});
        cnt--; 
    }

    void release(){
        unique_lock<mutex> l (m);
        cnt ++;
        cv.notify_one();
    }


};

//Class to create and controll a pair of chat room message reciver and sender threads
class ChatroomSystem {
    private:
        
        int memberCnt;
        int listeningPort;
        int chatRoomSocket;
        atomic<bool> shutdown;
        semaphore shutdownComplete;
        vector<int>  roomMemberList;
        mutex memberListMutex;
       
        //Struct to pass to sender threads with messager and the senders fd 
        struct outgoingMessage{
            int senderFD;
            char message[MAX_DATA];
        };
        semaphore messageQueueSem;
        vector<outgoingMessage> messageQueue;
        mutex messageQueueMutex;
        string roomName;
        
    public:
    
    //Defualt Constructor takes the chat room anme 
    ChatroomSystem(string _roomName):memberCnt(0),listeningPort(0),chatRoomSocket(0), shutdown(false), shutdownComplete(0),
    roomMemberList(),messageQueue(), messageQueueSem(0),messageQueueMutex(),roomName(_roomName){
        
    }
   
    //Member Acessor funtions
    int getMemberCnt(){
        return roomMemberList.size();
    }
    
    int getPortNo(){
        return listeningPort;
    }
    
    string getRoomName(){
        return roomName;
    }
   
    //thread function that will loop endlessly removing dead clients and check for sent messages using select  
    void chatRoomRecieverThread(){
        
#ifdef _DEBUG
    printf("Chat room reciever thread started\n");
#endif
        struct sockaddr_storage incommingAddr;
        
        socklen_t incommingAddrLen = sizeof(incommingAddr);
        char messageBuff[MAX_DATA];
        
        while(true){
            
#ifdef _DEBUG
    printf("Chat room reciever thread looping\n");
#endif
            //clear and set up active listening fd set every loop
            fd_set rfds;
            struct timeval tv;
            int retval;
            
            FD_ZERO(&rfds);
            memberListMutex.lock();
            
            FD_SET(chatRoomSocket,&rfds);
            
            int maxSd = chatRoomSocket;
            
            for(int i =0; i< roomMemberList.size(); i++){
                
                FD_SET(roomMemberList[i],&rfds);
            }
            
            memberListMutex.unlock();
            
#ifdef _DEBUG
    printf("Chat room reciever thread select\n");
#endif
            retval = select(1023, &rfds,NULL,NULL,NULL);
            if(retval == -1){
                cerr<<"Chatroom Select"<<endl;
            }
            
#ifdef _DEBUG
            printf("Chat Room Activity room:%s\n",roomName);
#endif //_DEBUG
        
        
            //take in new coonections first if any
            if(FD_ISSET(chatRoomSocket,&rfds)){
                
                if(!shutdown){//normal action on fd
#ifdef _DEBUG
                printf("Chat Room %s new connection\n",roomName);
#endif //_DEBUG
                       
                    memberListMutex.lock(); 
                    
#ifdef _DEBUG
                printf("Chat Room %s member list mutex lock\n",roomName);
#endif //_DEBUG
                    //accept and add new member connection
                    int incommingSocket = accept(chatRoomSocket, (struct sockaddr*)&incommingAddr, &incommingAddrLen);
                    if(incommingSocket == -1){
                        
                        cerr<<"Server Error accept"<<endl;
                        continue;
                    }
                    else {
                        
                        roomMemberList.push_back(incommingSocket);
#ifdef _DEBUG
                        printf("Chat room new connection accepted\n");
#endif
                    }
                    memberListMutex.unlock();
                    
                }
                else{//begin shutdown main socket closed
#ifdef _DEBUG
                    printf("Chat Room %s shutdown\n",roomName);
#endif //_DEBUG
                   //terminate the thread
                   return;
                    
                }
                
            }
#ifdef _DEBUG
            printf("Chat room chekcing room member activity\n");
#endif 

            //Handle events on other sockets
            memberListMutex.lock();
            auto itr = roomMemberList.begin();
            while(itr != roomMemberList.end() ){
                
                if (FD_ISSET(*itr, &rfds)){
                    //activity detected
#ifdef _DEBUG
                    printf("Chat Room %s activity detected\n",roomName);
#endif //_DEBUG
                    
                    if((retval= read(*itr,messageBuff,MAX_DATA))==0){
                        //Empty read client has disonnected remove from list
#ifdef _DEBUG
                        printf("Chat Room %s member disconnected:%d\n",roomName, *itr);
#endif //_DEBUG
                        close(*itr) ;
                        itr = roomMemberList.erase(itr);
                        
                    }
                    else{//Message recieved put into outward message queue 
#ifdef _DEBUG
                        printf("Chat Room %s member:%d message recieved:%d\n",roomName, *itr,messageBuff);
#endif //_DEBUG
                        outgoingMessage toSend;
                        strcpy(toSend.message, messageBuff);
                        toSend.senderFD = *itr;
                        
                        messageQueueMutex.lock();
                        
                        messageQueue.push_back(toSend);
                        
                        messageQueueSem.release();
                        messageQueueMutex.unlock();
                        ++itr;
                        
                    }
                    
                }
                else{
                    
                    ++itr;
                }
                
                
            }
            
            memberListMutex.unlock();
        } 
        
    }
   
    //thread function that continously waits for outgoing messages and will send them out to all connected users besides the sender 
    void chatRoomSenderThread(){
       
#ifdef _DEBUG
    printf("Chat room sender thread started\n");
#endif
         
        while(1){
            
            messageQueueSem.acquire();
#ifdef _DEBUG
            printf("Chat Sender message acquired\n");
#endif //_DEBUG

            memberListMutex.lock();
            messageQueueMutex.lock();
            int sender = messageQueue.front().senderFD;
            char outBuff[MAX_DATA];
            strcpy(outBuff,messageQueue.front().message);
            messageQueue.erase(messageQueue.begin());
            messageQueueMutex.unlock();
            
            //termination message recieved send shutdown to all members and terminate#
            if(outBuff[MAX_DATA-1]== '@'){
#ifdef _DEBUG
                printf("Sender Thread termination recieved\n");
#endif //DEBUG
                for(int i =0; i <roomMemberList.size(); i++){
                    
                    send(roomMemberList[i], outBuff,MAX_DATA,0);
                }
                
                memberListMutex.unlock();
                shutdownComplete.release();
                return;
            }
#ifdef _DEBUG
            printf("Outward message:%s\n",outBuff);
            int cnt=0;
            while(outBuff[cnt] != '\0'){
                
                if(outBuff[cnt] == '\0'){
                    printf("It hast terninator\n");
                }
                
                cnt++;
            }
#endif

            //Normal activity, format message to send out to call connected clients
            outBuff[MAX_DATA-1]= '|';
            outBuff[MAX_DATA-2]= '\0';
            for(int i =0 ; i < roomMemberList.size(); i++){
               
               if(roomMemberList[i]== sender) {
                   
                   continue;
               }
               
               send(roomMemberList[i],outBuff,MAX_DATA,0);
            }
            
            memberListMutex.unlock();
        }
        
    }
   
    //Initalizes chat room by listening to an available port and launch the sender and reciever threads 
    void chatRoomStart(){
        
#ifdef _DEBUG
        printf("Chat Room Starting\n");
#endif //_DEBUG

        memberListMutex.lock();
        memberListMutex.unlock();

        int masterFd=-1;

        //create socket
        struct sockaddr_in addr={0};
        addr.sin_family = AF_INET;
        addr.sin_port =htons(0);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        masterFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(masterFd==-1){
            perror("Chatromm socket failure");
            exit(-1);
        }
        
        if(bind(masterFd, (struct sockaddr*)&addr,sizeof(addr))==-1){
            perror("Chat room bind error");
            exit(-1);
        }
        if(listen(masterFd,20) ==-1){
            perror("Chat room listen error");
            exit(-1);
        }
        
        //setup listening on new port ranomly by binding to port 0
        int portNum =0;
    
        struct sockaddr_in sain;
        
        socklen_t len = sizeof(sain);
        
        if (getsockname(masterFd, (struct sockaddr *)&sain, &len) == -1){
            perror("getsockname");
            exit(-1);
        }
        else{
#ifdef _DEBUG
            printf("port number %d\n", ntohs(sain.sin_port));
#endif //_DEBUG
        }
        
        portNum = ntohs(sain.sin_port); 
        
        chatRoomSocket = masterFd;
        
    
#ifdef _DEBUG
    printf("Chat Room bound to port:%d\n",portNum);
#endif //_DEBUG
    
        listeningPort = portNum;
        thread r(&ChatroomSystem::chatRoomRecieverThread,this);
        r.detach();
        
        thread s(&ChatroomSystem::chatRoomSenderThread,this);
        s.detach();
        
#ifdef _DEBUG
    printf("Chat room threads launched\n");
#endif //_DEBUG
    
    }
    
    void chatRoomShutDown(){
        
#ifdef _DEBUG
        printf("Chat Room Shutting Down Command\n");
#endif 
       shutdown = true;
       
       close(chatRoomSocket);

       char messageBuff[MAX_DATA];
        
       messageQueueMutex.lock(); 
       outgoingMessage toClose;
       toClose.senderFD =-1; 
       for(int i =0; i < MAX_DATA; i++){
           messageBuff[i] = '@';
       }
       strcpy(toClose.message,messageBuff);
       messageQueue.push_back(toClose);
       messageQueueSem.release();
       messageQueueMutex.unlock();
       
       shutdownComplete.acquire();
       for(int i = 0 ; i <roomMemberList.size(); i ++){
           close(roomMemberList[i]);
       }
       
#ifdef _DEBUG
        printf("Chat Room Shutting Down Complete\n");
#endif 
       return;
    }
};

//set to contain the created chat rooms in key value pairs of room name and ChatRoom Object
unordered_map<string ,ChatroomSystem*> chatRoomList;
//mutex for acessing the chatRoomList.
mutex chatRoomListMutex;

//function to translate recieved message into command and name
char decodeCommand(char* commandBuff,int len,string *targetName){
    
    char compBuff[MAX_DATA];
    
    strcpy(compBuff,commandBuff);
    touppercase(compBuff,7);
    
    
    //determine if revieced command is proper format 
    int nameStart=0;
    char fChar= 'X';
    if(strncmp(compBuff, "JOIN", 4)==0){
        
        fChar ='J';
        nameStart =5;
    }
    else if(strncmp(compBuff, "CREATE", 6)==0){
        
        fChar ='C';
        nameStart =7;
    }
    else if(strncmp(compBuff, "DELETE", 6)==0){
        
        fChar ='D';
        nameStart =7;
    }
    else if(strncmp(compBuff, "LIST", 4)==0){
        
        fChar ='L';
        return fChar;
    }
    else{
        fChar ='X';
        return fChar;
    }
        
    
    
    
    //next if not list chekc if null terminated string and provided name is at least 1 char long
    if(commandBuff[len-1] == '\0' && len >2){
        
        //create output string
        int i=nameStart;
        char currChar = commandBuff[i];
        while(currChar != '\0'){
            
            currChar=commandBuff[i];
            *targetName+= currChar;
            i++;
            
        }
        
        if(fChar == 'C' || fChar == 'D' || fChar == 'J'){
            
            return fChar;    
        }
        
        
    }
    
    //return invalid command otherwise
    return 'X'; 
   
}


//thread function to process incomming client connections and commands
//doesn't loop so clients must reconnect to send new commands
void incommingConnectionHandler(int incommingSocket){
    
    
#ifdef _DEBUG
    printf("Incomming Connection handler started incomming socket:%d\n",incommingSocket);
#endif //_DEBUG
    
    char buff[MAX_DATA];
    int messageSize=-1;
    string targetName="";
    
    //after accepting an incomming client wait for sent server command
    if( (messageSize =recv (incommingSocket, buff, sizeof(buff), 0)) < 0){
        
        cerr<< "Incomming Connection Thread Error recieve failed"<<endl;
        exit(-1);
    }
    
    if(messageSize ==0){//empty command recieved
        
    }
    char commType = decodeCommand(buff, messageSize,&targetName );
    

#ifdef _DEBUG
    printf("Incomming Connection recieved command:%s\n",buff);
#endif //_DEBUG
    
    //process the recieved server command and create reply
    Reply reply;
    reply.status=FAILURE_UNKNOWN;
    reply.port= -1;
    
    chatRoomListMutex.lock();
    if(commType == 'C'){//check if chat room exists and create if otherwise
    
        if(chatRoomList.find(targetName) != chatRoomList.end()){
            
            reply.status = FAILURE_ALREADY_EXISTS;
        }
        else{
            ChatroomSystem* tempCRS = new ChatroomSystem(targetName);
            tempCRS->chatRoomStart();
            chatRoomList.insert({targetName,tempCRS});
            reply.status = SUCCESS;
    #ifdef _DEBUG
            printf("Chat room added current list len:%d \n List:",chatRoomList.size());
            for(auto i = chatRoomList.begin(); i != chatRoomList.end(); ++i){
               printf(" %s, ",(char*)i->second->getRoomName().c_str()) ;
            }
            printf("\n");
    #endif
            
        }
        
    }
    else  if(commType=='D'){
        
        if(chatRoomList.find(targetName) != chatRoomList.end())  {
            
            
            ChatroomSystem* temp  =chatRoomList[targetName];
            temp->chatRoomShutDown();
            chatRoomList.erase(targetName);
            delete temp;
            reply.status = SUCCESS;
            
        }
        else{
            reply.status = FAILURE_NOT_EXISTS;
        }
        
    }
    else  if(commType=='L'){
        
        string nameList="";
        
        //empty list send back empty 
        if(chatRoomList.size() ==0){
            reply.list_room[0] = 'e';
            reply.list_room[1] = 'm';
            reply.list_room[2] = 'p';
            reply.list_room[3] = 't';
            reply.list_room[4] = 'y';
            reply.list_room[5] = '\0';
            reply.status = SUCCESS;
            
        }else{
        
#ifdef _DEBUG
            printf("Server Chatroom cnt:%d\n",chatRoomList.size());
#endif 
            int cnt=0;
    
            for(auto itr = chatRoomList.begin(); itr != chatRoomList.end(); ++itr){
                
                for(int i = 0; i <itr->second->getRoomName().size(); i++) {
                    if(itr->second->getRoomName()[i] != '\0' && itr->second->getRoomName()[i] != '\n'){
                        reply.list_room[cnt] = itr->second->getRoomName()[i];
    #ifdef _DEBUG
                    cout<<"Char:"<<itr->second->getRoomName()[i];
    #endif
                        cnt++;
                    }
                }
                reply.list_room[cnt]= ' ';
                cnt++;
            }
            reply.list_room[cnt]='\0';
    #ifdef _DEBUG
            printf("Room List:%s\n",reply.list_room);
    #endif
            reply.status = SUCCESS;
        }
            
    }
    else if(commType == 'J'){
        
        if(chatRoomList.find(targetName) != chatRoomList.end()){
            
            //Room exists
            
            ChatroomSystem* temp = chatRoomList[targetName];
                
            reply.num_member =  temp->getMemberCnt();
            reply.port = temp->getPortNo();
            reply.status = SUCCESS;
        }
        else{
            reply.status = FAILURE_NOT_EXISTS;
            reply.port =-1;
            
        }
        
        
    }
    else{//invalid command recieved
    
        reply.status = FAILURE_INVALID;
        
    }
    chatRoomListMutex.unlock();
    //send reply structure to client
    if(send(incommingSocket, &reply, sizeof(reply), 0) == -1){
        
        cerr << "Incomming Connection Thread Error send failed"<<endl;
        exit(-1);
    }
    
#ifdef _DEBUG
    printf("Incomming Connection closing client socket:%d\n",incommingSocket);
#endif //_DEBUG
    
    close(incommingSocket);
    
    return;
}

//Main function to establish master socket on port argv[1] to listen and dispatch connection manager threads
int main( int argc, char* args[]){
    if (argc != 2){
        
        printf("Server Error Need 1 port argument\n");
        exit (-1);
    }

    string portno = args[1];

#ifdef _DEBUG
    printf("Listening on port:%d\n",portno);
#endif //_DEBUG
    
    //setup listening on master port from argument
    int masterFd, incommingFD;
    struct addrinfo hints, *result;
    struct sockaddr_storage incommingAddr;
    socklen_t incommingAddrLen = sizeof(incommingAddr);
    char ip[INET6_ADDRSTRLEN], buff[MAX_DATA];
    int returnValue;
    
   
    //initalize addrinfo hints 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    
    if( (returnValue = getaddrinfo(NULL,(char*) portno.c_str(), &hints, &result)) != 0){
        
        cerr <<"Server Error getaddrinfo: "<<gai_strerror(returnValue) <<endl;
        exit(-1);
    }
    masterFd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    
#ifdef _DEBUG
    printf("Ai family:%d, Ai protocol:%d, Ai address:%d\n", result->ai_family, result->ai_protocol, result->ai_addr);
#endif

    if(masterFd == -1){
        
        cerr << "Server Error socket"<<endl;
        exit(-1);
    }
    
     
    if(bind(masterFd, result->ai_addr, result->ai_addrlen) == -1){
        
        close(masterFd);
        cerr<< "Server Error bind"<<endl;
        exit(-1);
    }
    
    
    freeaddrinfo(result);
    
    //begin listening on established mater port
    if(listen(masterFd, 20) == -1){
        
        cerr<<" Server Error listen"<<endl;
        exit(-1);
    }
    
#ifdef _DEBUG
    printf("Master socket established entering main listening loop . . .\n");
#endif //_DEBUG
    
    //enter main loop and wait for incomming connections to accept
    while(true){
        
        int incommingSocket = accept(masterFd, (struct sockaddr*)&incommingAddr, &incommingAddrLen);
        if(incommingSocket == -1){
            
            cerr<<"Server Error accept"<<endl;
            continue;
        }
        
#ifdef _DEBUG
    printf("Incomming connection accepted, creating connectionHandlerThread\n");
#endif //_DEBUG

        thread t(&incommingConnectionHandler , incommingSocket);
        t.detach();
    }
    
    return 0;

}

#undef _DEBUG