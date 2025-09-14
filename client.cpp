#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <atomic>
//#include <netinet/tcp.h>

// KEEP ALIVE Listner Thread mechanism
void receiver(int sock, std::atomic<bool>& connected) {
    char buffer[1024];
    while (connected) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(sock, buffer, sizeof(buffer) - 1);

        if (bytes_read <= 0) {
            std::cout << "\n[INFO] Disconnected from server. Press Enter to exit.\n";
            connected = false; // Signal main thread to exit
            break;
        }

        std::string server_msg(buffer);

        // Check if the message is a KEEP_ALIVE ping.
        if (server_msg.find("KEEP_ALIVE") != std::string::npos) {
            // Automatically respond without bothering the user.
            std::string response = "ALIVE_OK\n";
            send(sock, response.c_str(), response.size(), 0);
        } else {
            // It's a normal message (question, feedback, etc.), so print it.
            std::cout << server_msg << std::flush;
        }
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cout << "Socket creation error\n";
        return -1;
    }

    // Disable Nagle's Algorithm - this is good practice for interactive apps.
    // int opt = 1;
    // if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0) {
    //     std::cerr << "Error: Unable to set TCP_NODELAY\n";
    // }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "192.168.50.111", &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Connection Failed\n";
        return -1;
    }

    std::cout << "Connected to server\n";

    std::atomic<bool> connected{true};
    std::thread receiver_thread(receiver, sock, std::ref(connected));
    
    // The main thread's job is now very simple:
    // 1. Read a line from the user.
    // 2. Send it to the server.
    // 3. Repeat.
    std::string user_input;
    while (connected) {
        std::getline(std::cin, user_input);
        if (!connected) {
            break;
        }
        
        user_input += "\n"; // Add newline for server-side parsing
        if (send(sock, user_input.c_str(), user_input.size(), 0) < 0) {
            std::cout << "[ERROR] Failed to send message.\n";
            break;
        }
    }

    std::cout << "Exiting client program.\n";
    connected = false;
    shutdown(sock, SHUT_RDWR); // Gracefully close the connection
    receiver_thread.join(); // Wait for the receiver thread to finish
    close(sock);
    return 0;
}

// int main() {
//     // CLIENT SOCKET
//     int sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) {
//         std::cout << "Socket creation error\n";
//         return -1;
//     }
//     std::cout << "Socket created\n";

//     // 2. CRITICAL FIX: Disable Nagle's Algorithm to prevent message bundling
//     int opt = 1;
//     if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt)) < 0) {
//         std::cerr << "Error: Unable to set TCP_NODELAY\n";
//     }

//     struct sockaddr_in serv_addr;
//     memset(&serv_addr, 0, sizeof(serv_addr));
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(8080);

//     if (inet_pton(AF_INET, "192.168.50.111", &serv_addr.sin_addr) <= 0) {
//         std::cout << "Invalid address\n";
//         return -1;
//     }

//     if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
//         std::cout << "Connection Failed\n";
//         return -1;
//     }

//     // CONNECTION ESTABLISHED
//     std::cout << "Connected to server\n";

//     // Create a shared flag and start the receiver thread
//     std::atomic<bool> connected{true};
//     std::thread receiver_thread(receiver, sock, std::ref(connected));
//     receiver_thread.detach(); // Let the thread run independently.

//     // The main thread now ONLY handles user input and sending.
//     // This single loop handles sending the genre AND all subsequent answers.
//     std::string user_input;

//     // 1. Handle the initial genre input
//     std::getline(std::cin, user_input);
//     if (connected) {
//         user_input += "\n";
//         send(sock, user_input.c_str(), user_input.size(), 0);
//     }

//     while (connected) {
//         // a. Get the user's ANSWER to the question
//         std::getline(std::cin, user_input);
//         if (!connected) break;
//         user_input += "\n";
//         if (send(sock, user_input.c_str(), user_input.size(), 0) < 0) break;

//         // b. Get the user's CHOICE for the leaderboard prompt
//         std::getline(std::cin, user_input);
//         if (!connected) break;

//         // Trim the input to check if it's "L" or "l"
//         std::string trimmed_input = user_input;
//         size_t endpos = trimmed_input.find_last_not_of(" \n\r\t");
//         if (std::string::npos != endpos) {
//             trimmed_input = trimmed_input.substr(0, endpos + 1);
//         }

//         user_input += "\n";
//         if (send(sock, user_input.c_str(), user_input.size(), 0) < 0) break;

//         // c. If they chose the leaderboard, there's one more input to handle
//         if (trimmed_input == "L" || trimmed_input == "l") {
//             // Wait for the user to press Enter to continue after viewing the board
//             std::getline(std::cin, user_input);
//             if (!connected) break;
//             user_input += "\n";
//             if (send(sock, user_input.c_str(), user_input.size(), 0) < 0) break;
//         }
//     }

//     std::cout << "Exiting client program.\n";
//     close(sock);
//     return 0;
// }

// int main(){
//     // CLIENT SOCKET
//     int sock= socket(AF_INET,SOCK_STREAM,0);
//     if(sock<0){
//         std::cout<<"Socket creation error\n";
//         return -1;
//     }
//     std::cout<<"Socket created\n";

//     struct sockaddr_in serv_addr;
//     memset(&serv_addr,0,sizeof(serv_addr));
//     serv_addr.sin_family= AF_INET;
//     serv_addr.sin_port= htons(8080);    

//     if(inet_pton(AF_INET,"192.168.50.111",&serv_addr.sin_addr)<=0){
//         std::cout<<"Invalid address\n";
//         return -1;
//     }

//     if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0){
//         std::cout<<"Connection Failed\n";
//         return -1;
//     }

//     // CONNECTION ESTABLISHED
//     char buffer[1024] = {0};    // To receive data from server

//     std::cout<<"Connected to server\n";

//     // Receive prompt for genre
//     memset(buffer,0,sizeof(buffer));
//     read(sock,buffer,sizeof(buffer));
//     std::cout<<buffer;

//     // Send genre once
//     std::string genre;
//     std::getline(std::cin, genre);
//     send(sock,genre.c_str(),genre.size(),0);

//     // Loop to receive and answer 10 questions
//     for(int i=0; i<10; ++i){
//         // Receive question / keep-alive / end
//         memset(buffer,0,sizeof(buffer));
//         int bytes_read = read(sock,buffer,sizeof(buffer));
//         if(bytes_read <= 0){
//             std::cout<<"Disconnected from server.\n";
//             break;
//         }
//         std::string server_msg(buffer);

//         // Handle KEEP-ALIVE
//         // if(server_msg.find("KEEP-ALIVE") != std::string::npos){
//         //     std::cout << "[Server heartbeat received]\n";
//         //     continue;
//         // }

//         // Handle Quiz Over
//         if(server_msg.find("Quiz over") != std::string::npos){
//             std::cout << server_msg << std::endl;
//             break;
//         }

//         // Otherwise it's a question
//         std::cout<<server_msg;

//         // Send answer
//         std::string ans;
//         std::cout<<"\nChoose your answer (A/B/C/D): ";
//         std::getline(std::cin, ans);
//         send(sock,ans.c_str(),ans.size(),0);

//         // Receive feedback
//         memset(buffer,0,sizeof(buffer));
//         bytes_read = read(sock,buffer,sizeof(buffer));
//         if(bytes_read <= 0){
//             std::cout<<"Disconnected from server.\n";
//             break;
//         }
//         std::cout<<buffer<<"\n";
//     }

//     close(sock);
//     return 0;   
// }