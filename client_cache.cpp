#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <atomic>

// KEEP ALIVE Listner Thread mechanism
void receiver(int sock, std::atomic<bool>& connected) {
    char buffer[2048];
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

// auto to work in AUTO MODE
// int main(int argc, char const *argv[]) {
//     bool is_auto_mode = false;
//     // Check if the program was run with the "auto" argument
//     if (argc > 1 && strcmp(argv[1], "auto") == 0) {
//         is_auto_mode = true;
//         // Seed the random number generator so each auto-client is unique
//         srand(time(0) + getpid()); 
//         std::cout << "[INFO] Client starting in AUTO mode.\n";
//     }

//     int sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) {
//         std::cerr << "Socket creation error\n";
//         return -1;
//     }

//     struct sockaddr_in serv_addr;
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(19033); // Using standard 8080 port

//     if (inet_pton(AF_INET, "192.168.50.111", &serv_addr.sin_addr) <= 0) {
//         std::cerr << "Invalid address/ Address not supported\n";
//         return -1;
//     }

//     if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
//         std::cerr << "Connection Failed\n";
//         return -1;
//     }

//     std::atomic<bool> connected{true};
//     std::thread receiver_thread(receiver, sock, std::ref(connected));

//     // A small delay to wait for the server's initial prompt
//     std::this_thread::sleep_for(std::chrono::milliseconds(200));

//     // --- STEP 1: CHOOSE A GENRE ---
//     std::string user_input;
//     if (is_auto_mode) {
//         const std::vector<std::string> genres = {"History", "Science", "Movies", "Sports", "Geography"};
//         user_input = genres[rand() % genres.size()];
//         std::cout << "[AUTO] Selecting genre: " << user_input << std::endl;
//     } else {
//         std::getline(std::cin, user_input);
//     }
//     user_input += "\n";
//     if (connected) send(sock, user_input.c_str(), user_input.size(), 0);

//     // --- STEP 2: ANSWER THE 10 QUIZ QUESTIONS ---
//     for (int i = 0; i < 10 && connected; ++i) {
//         // --- Part A: Answer the question ---
//         if (is_auto_mode) {
//             std::this_thread::sleep_for(std::chrono::seconds(2)); // Pause to simulate thinking
//             const char answers[] = {'A', 'B', 'C', 'D'};
//             user_input = std::string(1, answers[rand() % 4]);
//             std::cout << "[AUTO] Answering: " << user_input << std::endl;
//         } else {
//             std::getline(std::cin, user_input);
//         }
//         if (!connected) break;
//         user_input += "\n";
//         send(sock, user_input.c_str(), user_input.size(), 0);

//         // --- Part B: Decide whether to see the leaderboard ---
//         if (is_auto_mode) {
//             std::this_thread::sleep_for(std::chrono::seconds(1)); // Pause after answering
//             if (rand() % 3 == 0) { // 33% chance to check leaderboard
//                 user_input = "L";
//                 std::cout << "[AUTO] Checking leaderboard." << std::endl;
//             } else {
//                 user_input = ""; // Simulate pressing Enter to continue
//             }
//         } else {
//             std::getline(std::cin, user_input);
//         }
//         if (!connected) break;
//         user_input += "\n";
//         send(sock, user_input.c_str(), user_input.size(), 0);
        
//         // --- Part C: If we viewed the leaderboard, we must press Enter to continue ---
//         if (is_auto_mode && (user_input == "L\n" || user_input == "l\n")) {
//             std::this_thread::sleep_for(std::chrono::seconds(1));
//              user_input = "\n"; // Send an empty line to proceed
//             send(sock, user_input.c_str(), user_input.size(), 0);
//         }
//     }

//     // Wait for the receiver thread to finish cleanly
//     receiver_thread.join();
//     close(sock);
//     return 0;
// }

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cout << "Socket creation error\n";
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(13033);

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
