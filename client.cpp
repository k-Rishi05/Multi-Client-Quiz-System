#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(){
    // CLIENT SOCKET
    int sock= socket(AF_INET,SOCK_STREAM,0);
    if(sock<0){
        std::cout<<"Socket creation error\n";
        return -1;
    }
    std::cout<<"Socket created\n";

    struct sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family= AF_INET;
    serv_addr.sin_port= htons(8080);    

    if(inet_pton(AF_INET,"192.168.50.111",&serv_addr.sin_addr)<=0){
        std::cout<<"Invalid address\n";
        return -1;
    }

    if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0){
        std::cout<<"Connection Failed\n";
        return -1;
    }
    // CONNECTION ESTABLISHED
    char buffer[1024] = {0};    // To receive data from server

    std::cout<<"Connected to server\n";

    // Receive prompt for genre
    memset(buffer,0,sizeof(buffer));
    read(sock,buffer,sizeof(buffer));
    std::cout<<buffer;

    // Send genre once
    std::string genre;
    std::getline(std::cin, genre);
    send(sock,genre.c_str(),genre.size(),0);

    // Loop to receive and answer 10 questions
    for(int i=0; i<10; ++i){
        // Receive question
        memset(buffer,0,sizeof(buffer));
        int bytes_read = read(sock,buffer,sizeof(buffer));
        if(bytes_read <= 0){
            std::cout<<"Disconnected from server.\n";
            break;
        }
        std::cout<<buffer;

        // Send answer
        std::string ans;
        std::cout<<"\nChoose your answer (A/B/C/D): ";
        std::getline(std::cin, ans);
        send(sock,ans.c_str(),ans.size(),0);

        // Receive feedback
        memset(buffer,0,sizeof(buffer));
        bytes_read = read(sock,buffer,sizeof(buffer));
        if(bytes_read <= 0){
            std::cout<<"Disconnected from server.\n";
            break;
        }
        std::cout<<buffer<<"\n";
    }

    close(sock);
    return 0;   
}