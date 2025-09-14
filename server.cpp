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
#include <sstream>
#include <tuple>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <set> 

std::string reserve_llm(const std::string& rollno) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(25000);
    inet_pton(AF_INET, "192.168.50.142", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        std::cout<< "Connection to LLM server failed (reserve_llm)\n";
        return "";
    }
    // R"( )" is a raw string literal to avoid escaping quotes.
    std::string json = R"({"rollno":")" + rollno + R"("})";   // {"rollno":"es23btech11033"}
    std::string request =
        "POST /reserve HTTP/1.1\r\n"
        "Host: 192.168.50.142:25000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        json;

    send(sock, request.c_str(), request.size(), 0);

    char buffer[4096];
    std::string response;
    int bytes;
    while ((bytes = read(sock, buffer, sizeof(buffer)-1)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }
    close(sock);
    std:: cout<< "LLM Reserve Response: " << response << std::endl;
    // Strip HTTP headers. \r\n\r\n indicates end of headers after which CONTENT starts.
    // RETURN ONLY THE CONTENT
    size_t pos = response.find("\r\n\r\n");
    if (pos != std::string::npos)
        return response.substr(pos + 4);
    return response;
}

std::string query_llm(const std::string& rollno, const std::string& genre) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        std::cout<<"Socket creation failed\n";
        return "";
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(25000);
    inet_pton(AF_INET, "192.168.50.142", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        std::cout<<"Connection to LLM server failed (query_llm)\n";
        return "";
    }
    // R"( )" is a raw string literal to avoid escaping quotes.
    std::string json = R"({"rollno":")" + rollno + R"(","messages":[
        {"role":"system","content":"You are a quiz generator. Output exactly 10 multiple-choice questions as valid JSON only. Each question must have 4 options (A, B, C, D) and one correct answer. Return ONLY valid JSON in the following format: {\"questions\":[{\"q\":\"Question text\",\"options\":[\"A) ...\",\"B) ...\",\"C) ...\",\"D) ...\"],\"a\":\"A\"}, ...]}. The answer field 'a' must be only one of 'A', 'B', 'C', or 'D'. Do not include explanations, extra text, or markdown. Output only the JSON object."},
        {"role":"user","content":"Generate 10 MCQ trivia questions about )" + genre + R"(. Output only JSON."}],"temperature":0.0,"max_tokens":1200})";

    std::string request =
        "POST /generate_quiz HTTP/1.1\r\n"
        "Host: 192.168.50.142:25000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        json;

    send(sock, request.c_str(), request.size(), 0);

    char buffer[4096];
    std::string response;
    int bytes;
    while ((bytes = read(sock, buffer, sizeof(buffer)-1)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }
    close(sock);

    // Strip HTTP headers
    size_t pos = response.find("\r\n\r\n");
    if (pos != std::string::npos)
        return response.substr(pos + 4);
    return response;
}

std::mutex leaderboard_mutex;
std::map<int, int> leaderboard; // username -> score

// Track connected clients
std::mutex active_clients_mutex;
std::set<int> active_clients;

// std::string getLeaderboard(int client_id){
//     std::lock_guard<std::mutex> lock(leaderboard_mutex);
//     std::string msg="Leaderboard\n";
//     msg+="====================\n";

//     std::vector<std::pair<int,int> > score(leaderboard.begin(),leaderboard.end());
//     std::sort(score.begin(),score.end(),
//         [](auto &a,auto &b){
//             return a.second>b.second;
//         });
    
//         for(auto& it:score){
//             msg+=std::to_string(it.first)+" : "+std::to_string(it.second)+"\n";
//         }
//         msg+="====================\n";
//         msg+="\nYour ID: "+std::to_string(client_id)+ "\n"+ "Your Score: "+std::to_string(leaderboard[client_id])+"\n\n";
//         return msg;
// }
std::string getLeaderboard(int client_id, const std::set<int>& active_ids) {
    std::lock_guard<std::mutex> lock(leaderboard_mutex);
    std::stringstream msg;

    msg << "\033[2J\033[H";
    msg << "+---------------------------+\n";
    msg << "|   ACTIVE LEADERBOARD      |\n";
    msg << "+---------------------------+\n";
    msg << "|   Player ID   |   Score   |\n";
    msg << "+---------------------------+\n";

    std::vector<std::pair<int, int>> score_vec(leaderboard.begin(), leaderboard.end());
    std::sort(score_vec.begin(), score_vec.end(),
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
    
    for (const auto& score : score_vec) {
        // Only display the score if the client is in the active set.
        if (active_ids.count(score.first)) {
            msg << "| " << std::left << std::setw(13) << score.first
                << "| " << std::left << std::setw(9) << score.second << " |";
            if (score.first == client_id) {
                msg << "  <-- YOU";
            }
            msg << "\n";
        }
    }

    msg << "+---------------------------+\n";
    return msg.str();
}

// Returns vector of (question, options, answer)
std::vector<std::tuple<std::string, std::vector<std::string>, char>> parse_json_quiz(const std::string& text) {
    std::vector<std::tuple<std::string, std::vector<std::string>, char>> quiz;
    size_t pos = 0;
    //next occurrence of "{" in the string text, starting from the index pos
    while ((pos = text.find("{", pos)) != std::string::npos) {
        // Find question
        size_t q_start = text.find("\"question\":", pos);
        if (q_start == std::string::npos) break;
        q_start = text.find("\"", q_start + 11) + 1;
        size_t q_end = text.find("\"", q_start);
        std::string question = text.substr(q_start, q_end - q_start);

        // Find options
        size_t opt_start = text.find("[", q_end);
        size_t opt_end = text.find("]", opt_start);
        std::vector<std::string> options;
        size_t curr = opt_start + 1;
        char correct_option = 'A';
        int correct_index = -1;
        int idx = 0;
        while (curr < opt_end) {
            size_t quote1 = text.find("\"", curr);
            size_t quote2 = text.find("\"", quote1 + 1);
            if (quote1 == std::string::npos || quote2 == std::string::npos || quote2 > opt_end) break;
            std::string opt = text.substr(quote1 + 1, quote2 - quote1 - 1);
            options.push_back(std::string(1, 'A' + idx) + ") " + opt);
            curr = quote2 + 1;
            idx++;
        }

        // Find answer
        size_t ans_start = text.find("\"answer\":", opt_end);
        if (ans_start == std::string::npos){
            std::cout<< "Answer field not found\n";
            break;
        } 
        ans_start = text.find("\"", ans_start + 9) + 1;
        size_t ans_end = text.find("\"", ans_start);
        std::string answer = text.substr(ans_start, ans_end - ans_start);

        // Find which option matches answer
        for (int i = 0; i < options.size(); ++i) {
            // Remove "A) ", "B) ", etc. for comparison
            std::string opt_text = options[i].substr(3);
            if (opt_text == answer) {
                correct_option = 'A' + i;
                break;
            }
        }

        quiz.push_back({question, options, correct_option});
        pos = ans_end;
    }
    return quiz;
}

// Extracts the value of "content":" from the LLM JSON response
std::string extract_content_field(const std::string& json) {
    std::string key = R"("content":")";
    size_t start = json.find(key);
    if (start == std::string::npos){
        std::cout<< "Content field not found in response\n";
        return "";
    } 
    start += key.length();

    std::string content;
    bool escape = false;
    for (size_t i = start; i < json.size(); ++i) {
        char c = json[i];
        if (escape) {
            switch (c) {
                case 'n': content += '\n'; break;
                case 't': content += '\t'; break;
                case 'r': content += '\r'; break;
                case '\\': content += '\\'; break;
                case '"': content += '"'; break;
                default: content += c; break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"' && !escape) {
            // Check if this is the end of the content field
            // Look ahead for , or } to confirm end of string
            // This is a bit hacky but works for this context
            break;
        } else {
            content += c;
        }
    }
    return content;
}

// For KEEP-ALIVE mechanism
void keep_alive_checker(int client_socket, int client_id, std::atomic<bool>& client_is_alive, std::atomic<std::chrono::steady_clock::time_point>& last_message_time) {
    while (client_is_alive) {
        // Wait for 5 seconds before the next check.
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!client_is_alive) break;

        // Check for timeout: if no message has been received for over 10 seconds.
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_message_time.load()).count();

        if (elapsed > 10) {
            std::cout << "[INFO] Client " << client_id << " timed out. Closing connection.\n";
            client_is_alive = false;
            // Closing the socket will cause the main thread's blocking read() to fail.
            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
            break;
        }

        // Send the KEEP_ALIVE ping.
        std::string ping = "KEEP_ALIVE\n";
        if (send(client_socket, ping.c_str(), ping.size(), 0) <= 0) {
            if (client_is_alive) { // Avoid double message if already closed
                 std::cout << "[INFO] Client " << client_id << " disconnected (ping failed).\n";
                 client_is_alive = false;
                 shutdown(client_socket, SHUT_RDWR);
                 close(client_socket);
            }
            break;
        }
    }
}

// Correct one
// void handle_client(int client_socket, int client_id){

//     char buffer[1024] = {0};
//     auto start = std::chrono::steady_clock::now();

//     // Ask for genre
//     std::string prompt = "Enter quiz genre:\n";
//     send(client_socket, prompt.c_str(), prompt.size(), 0);

//     // Receive genre
//     memset(buffer, 0, sizeof(buffer));
//     int bytes_read = read(client_socket, buffer, sizeof(buffer));
//     if (bytes_read <= 0) {
//         std::cout << "Client disconnected before genre: " << client_socket << "\n";
//         close(client_socket);
//         return;
//     }
//     std::string genre(buffer);
//     std::cout << "[DEBUG] Received genre: " << genre << std::endl;

//     // Get quiz from LLM (as JSON string)
//     std::string quiz_response = query_llm("es23btech11033", genre);
//     std::cout << "[DEBUG] LLM raw response: " << quiz_response << std::endl;

//     // Extract the quiz text from the JSON response
//     std::string quiz_text = extract_content_field(quiz_response);
//     std::cout << "[DEBUG] Extracted quiz text:\n" << quiz_text << std::endl;
//     auto quiz = parse_json_quiz(quiz_text);
//     std::cout << "[DEBUG] Parsed quiz size: " << quiz.size() << std::endl;
//     if (quiz.empty()) {
//         std::string error_msg = "Error generating quiz. Please try again.\n";
//         send(client_socket, error_msg.c_str(), error_msg.size(), 0);
//         close(client_socket);
//         return;
//     }

//     // Send questions one by one
//     for (const auto& [question, options, answer] : quiz) {

//         // Check time limit (5 mins)
//         auto now = std::chrono::steady_clock::now();
//         auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - start).count();
//         if (elapsed >= 1) break;


//         // Send question
//         std::string q_text = question + "\n";
//         for (const auto& opt : options) q_text += opt + "\n";
//         send(client_socket, q_text.c_str(), q_text.size(), 0);

//         // Receive answer
//         memset(buffer, 0, sizeof(buffer));
//         int ans_bytes = read(client_socket, buffer, sizeof(buffer));
//         if (ans_bytes <= 0) break;
//         std::string client_ans(buffer);

//         // Feedback
//         std::string msg;
//         if (!client_ans.empty() && toupper(client_ans[0]) == answer) {
//             msg = "Correct Answer!\n";
//             std::lock_guard<std::mutex> guard(leaderboard_mutex);
//             leaderboard[client_id]++;
//         } else {
//             msg = std::string("Wrong Answer! Correct answer is ") + answer + "\n";
//         }
//         send(client_socket, msg.c_str(), msg.size(), 0);

//         sleep(3); // Wait for 3 seconds before next question
//     }

//     // After quiz ends or time limit reached
//     std::string end_msg = "Quiz over – final scores available\n";
//     end_msg += getLeaderboard(client_id);
//     send(client_socket, end_msg.c_str(), end_msg.size(), 0);
    
//     close(client_socket);
// }


void handle_client(int client_socket, int client_id) {
    char buffer[1024] = {0};
    auto start = std::chrono::steady_clock::now();

    // Ask for and receive genre (No watchdog needed here yet)
    std::string prompt = "Enter quiz genre:\n";
    send(client_socket, prompt.c_str(), prompt.size(), 0);

    memset(buffer, 0, sizeof(buffer));
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        std::cout << "Client disconnected before genre: " << client_id << "\n";
        close(client_socket);
        return;
    }
    std::string genre(buffer);
    genre.erase(genre.find_last_not_of(" \n\r\t") + 1); // Keep the trim fix
    std::cout << "[DEBUG] Received genre: " << genre << std::endl;

    // Get quiz from LLM - THIS IS THE LONG-RUNNING TASK
    std::string quiz_response = query_llm("es23btech11033", genre);
    std::cout << "[DEBUG] LLM raw response: " << quiz_response << std::endl;
    std::string quiz_text = extract_content_field(quiz_response);
    std::cout << "[DEBUG] Extracted quiz text:\n" << quiz_text << std::endl;
    auto quiz = parse_json_quiz(quiz_text);
    
    if (quiz.empty()) {
        std::string error_msg = "Error generating quiz. Please try again.\n";
        send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        close(client_socket);
        return;
    }
    //std::cout << "[DEBUG] Parsed quiz" << quiz.size() << std::endl;


    // <<< CHANGE: START THE KEEP-ALIVE MECHANISM HERE, AFTER SETUP IS DONE >>>
    std::atomic<bool> client_is_alive{true};
    std::atomic<std::chrono::steady_clock::time_point> last_message_time;
    last_message_time = std::chrono::steady_clock::now();
    std::thread keep_alive_th(keep_alive_checker, client_socket, client_id, std::ref(client_is_alive), std::ref(last_message_time));

    // Send questions one by one
    for (const auto& [question, options, answer] : quiz) {
        // Check if keep-alive thread detected a disconnect
        if (!client_is_alive) break;

        // Check time limit (5 mins)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - start).count();
        if (elapsed >= 5) break; // Use 5 for a real quiz

        // Send question (clear screen first for better UX)
        std::string q_text = "\033[2J\033[H"+ question + "\n";
        for (const auto& opt : options) q_text += opt + "\n";
        q_text += "\nChoose the correct option: ";
        send(client_socket, q_text.c_str(), q_text.size(), 0);

        // <<< CHANGE 4: Replace the simple read() with this intelligent loop >>>
        std::string client_ans;
        while (client_is_alive) {
            memset(buffer, 0, sizeof(buffer));
            int ans_bytes = read(client_socket, buffer, sizeof(buffer) - 1);
            
            if (ans_bytes <= 0) {
                client_is_alive = false;
                break;
            }

            last_message_time = std::chrono::steady_clock::now();
            std::string received(buffer);
            received.erase(received.find_last_not_of(" \n\r\t") + 1);

            if (received == "ALIVE_OK") {
                // Heartbeat received, continue waiting for the actual answer.
                continue;
            } else {
                // Actual answer received, store it and break this waiting loop.
                client_ans = received;
                break;
            }
        }

        // Exit the main quiz loop if the client has disconnected.
        if (!client_is_alive) break;

        // Feedback
        std::string msg;
        if (!client_ans.empty() && toupper(client_ans[0]) == answer) {
            msg = "Correct Answer!\n\n";
            std::lock_guard<std::mutex> guard(leaderboard_mutex);
            leaderboard[client_id]++;
        } else {
            msg = std::string("Wrong Answer! Correct answer is ") + answer + "\n\n";
        }
        send(client_socket, msg.c_str(), msg.size(), 0);

        //sleep(2); // A small delay before the next question is fine.

        std::string prompt = "\nPress [L] for Leaderboard, or [Enter] for Next Question: ";
        send(client_socket, prompt.c_str(), prompt.size(), 0);

        std::string choice;
        while (client_is_alive) {
            memset(buffer, 0, sizeof(buffer));
            int choice_bytes = read(client_socket, buffer, sizeof(buffer) - 1);
            if (choice_bytes <= 0) { client_is_alive = false; break; }
            last_message_time = std::chrono::steady_clock::now();
            std::string received(buffer, choice_bytes);
            received.erase(received.find_last_not_of(" \n\r\t") + 1);
            if (received != "ALIVE_OK") {
                choice = received;
                break; // Got the real choice, exit loop
            }
            // If it was ALIVE_OK, loop and read again
        }
        if (!client_is_alive) break;

        if (choice == "L" || choice == "l") {
            std::string board;
            { // Lock active_clients to safely read it
                std::lock_guard<std::mutex> lock(active_clients_mutex);
                board = getLeaderboard(client_id, active_clients);
            }
            send(client_socket, board.c_str(), board.size(), 0);
            
            // Wait for user to press Enter to continue
            std::string continue_prompt = "\n--- Leaderboard Displayed ---\nPress [Enter] to continue...";
            send(client_socket, continue_prompt.c_str(), continue_prompt.size(), 0);
            while (client_is_alive) {
                memset(buffer, 0, sizeof(buffer));
                int enter_bytes = read(client_socket, buffer, sizeof(buffer) - 1);
                if (enter_bytes <= 0) { client_is_alive = false; break; }
                last_message_time = std::chrono::steady_clock::now();
                std::string received(buffer, enter_bytes);
                received.erase(received.find_last_not_of(" \n\r\t") + 1);
                if (received != "ALIVE_OK") {
                    break; // Got the Enter key, exit loop
                }
            }
        }
    }

    // After quiz ends or time limit reached
    if (client_is_alive) {
        std::string end_msg = "Quiz over – final scores available\n";
        end_msg += getLeaderboard(client_id, active_clients);
        send(client_socket, end_msg.c_str(), end_msg.size(), 0);
    }
    
    // <<< CHANGE 5: Cleanly shut down the keep-alive thread >>>
    client_is_alive = false; // Signal the thread to exit its loop
    keep_alive_th.join();    // Wait for the thread to finish
    
    // Now that the session is truly over, remove the client from the active set.
    {
        std::lock_guard<std::mutex> lock(active_clients_mutex);
        active_clients.erase(client_id);
    }
    std::cout << "[INFO] Client " << client_id << " session finished and resources cleaned up.\n";
}


int main(){
    
    reserve_llm("es23btech11033"); // Reserve LLM instance
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
        std::cout << "Client connected: " << new_socket << " with ID: " << client_id << "\n";
        // Add the new client to the active set
        {
            std::lock_guard<std::mutex> lock(active_clients_mutex);
            active_clients.insert(client_id);
        }
        std::thread t(handle_client,new_socket,client_id++);
        t.detach(); // Detach to run thread for each client
    }

    return 0;
}