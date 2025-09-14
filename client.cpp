#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <atomic>

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

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(19033);

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
