#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>

std::mutex leaderboard_mutex;
std::map<int, int> leaderboard; // username -> score

std::string getLeaderboard(int client_id){
    std::lock_guard<std::mutex> lock(leaderboard_mutex);
    std::string msg="Leaderboard\n";
    msg+="====================\n";

    std::vector<std::pair<int,int> > score(leaderboard.begin(),leaderboard.end());
    std::sort(score.begin(),score.end(),
        [](auto &a,auto &b){
            return a.second>b.second;
        });
    
        for(auto& it:score){
            msg+=std::to_string(it.first)+" : "+std::to_string(it.second)+"\n";
        }
        msg+="====================\n";
        msg+="\nYour ID: "+std::to_string(client_id)+ "\n"+ "Your Score: "+std::to_string(leaderboard[client_id])+"\n\n";
        return msg;
}

void handle_client(int client_socket, int client_id){
    char buffer[1024] = {0}; // To receive data from client
    std::string q1="Who is Prime Minister of India?\nA. Narendra Modi\nB. Rahul Gandhi\nC. Arvind Kejriwal\nD. Manmohan Singh\n";
    std::string a1="A";
    while(true){
        // Receive genre
        memset(buffer,0,sizeof(buffer)); // clean old data
        read(client_socket,buffer,sizeof(buffer));
        std::string genre(buffer);
        // Check if client disconnected
        if(genre==""){
            std::cout<<"Client disconnected: "<<client_socket<<"\n";
            break;
        }

        // Check if Client requested leaderboard
        if(genre=="LEADERBOARD"){
            std::string lb=getLeaderboard(client_id);
            send(client_socket,lb.c_str(),lb.size(),0);
            continue;
        }

        // Send question based on genre
        //need to prompt LLM using API key to get the question and answer based on genre and send to client
        send(client_socket,q1.c_str(),q1.size(),0);
        memset(buffer,0,sizeof(buffer)); // clean old data

        // Receive answer
        read(client_socket,buffer,sizeof(buffer));
        // Check answer
        std::string msg;
        if(strcmp(buffer,a1.c_str())==0){
            msg="Correct Answer!\n";
            std::lock_guard<std::mutex> guard(leaderboard_mutex);
            leaderboard[client_id]++; // Increment score
        }
        // else if(strcmp(buffer,"LEADERBOARD")==0){
        //     msg=getLeaderboard(client_id);
        // }
        else{
            msg="Wrong Answer! Correct answer is A. Narendra Modi\n";
        }
        send(client_socket,msg.c_str(),msg.size(),0);
    }

    close(client_socket);
}

int main(){
    // SERVER SOCKET
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1){
        std::cout<<"Socket creation failed\n";
        return -1;
    }
    std::cout<<"Socket created successfully\n";

    // IP AND PORT TO BIND
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family= AF_INET;
    address.sin_addr.s_addr= INADDR_ANY;
    address.sin_port= htons(8080);

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address))<0){
        std::cout<<"Binding failed\n";
        return -1;
    }
    std::cout<<"Binding successful\n";

    if(listen(server_fd,10)<0){
        std::cout<<"Listening failed\n";
        return -1;
    }
    std::cout<<"Listening\n";
    int new_socket;
    int addrlen= sizeof(address);
    int client_id=1;

    while(true){
        // ADDRESS IS OVERWRITTEN WITH CLIENT'S ADDRESS
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if(new_socket<0){
            std::cout<<"Accepting failed\n";
            return -1;
        }
        std::cout<<"Client connected :"<<new_socket<<"\n";
        std::thread t(handle_client,new_socket,client_id++);
        t.detach(); // Detach to run thread for each client
    }

    return 0;
}