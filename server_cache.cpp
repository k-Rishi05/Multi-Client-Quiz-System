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

// Represents a single cached quiz
struct CacheEntry {
    std::vector<std::tuple<std::string, std::vector<std::string>, char>> quiz_data;
    std::chrono::steady_clock::time_point creation_time;
};

// The main cache: maps a genre string to its CacheEntry
std::map<std::string, CacheEntry> quiz_cache;
std::mutex cache_mutex;
// Define the Time-To-Live for cache entries in minutes.
const int TTL_MINUTES = 10;

void handle_client(int client_socket, int client_id) {
    char buffer[1024] = {0};
    auto start = std::chrono::steady_clock::now();

    // Ask for and receive genre
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
    genre.erase(genre.find_last_not_of(" \n\r\t") + 1);
    
    // Vector to hold the final quiz questions, either from cache or new
    std::vector<std::tuple<std::string, std::vector<std::string>, char>> quiz;
    bool served_from_cache = false;

    // --- START CACHE CHECK ---
    { // Use a block to scope the lock_guard
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = quiz_cache.find(genre);

        if (it != quiz_cache.end()) {
            // Entry exists, now check if it's stale by comparing its age to TTL
            auto age = std::chrono::duration_cast<std::chrono::minutes>(
                std::chrono::steady_clock::now() - it->second.creation_time
            ).count();

            if (age < TTL_MINUTES) {
                // CACHE HIT: Entry is valid and not stale
                std::cout << "[INFO] Cache hit for genre: '" << genre << "'. Serving from cache.\n";
                quiz = it->second.quiz_data; // Copy data from cache
                served_from_cache = true;
            }
        }
    } // Mutex is automatically unlocked here

    if (!served_from_cache) {
        // CACHE MISS: Entry does not exist or is stale. Fetch from LLM.
        // The mutex is NOT held during this slow network operation.
        std::cout << "[INFO] Cache miss for genre: '" << genre << "'. Fetching from LLM.\n";

        std::string quiz_response = query_llm("es23btech11033", genre);
        std::string quiz_text = extract_content_field(quiz_response);
        quiz = parse_json_quiz(quiz_text);

        // If we got a valid quiz, add it to the cache
        if (!quiz.empty()) {
            std::lock_guard<std::mutex> lock(cache_mutex); // Lock again to write to the cache
            CacheEntry new_entry;
            new_entry.quiz_data = quiz;
            new_entry.creation_time = std::chrono::steady_clock::now();
            quiz_cache[genre] = new_entry;
            std::cout << "[INFO] Genre '" << genre << "' added to cache.\n";
        }
    }
    // --- END CACHE LOGIC ---

    if (quiz.empty()) {
        std::string error_msg = "Error generating or finding quiz for this genre. Please try another.\n";
        send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        close(client_socket);
        return;
    }

    std::atomic<bool> client_is_alive{true};
    std::atomic<std::chrono::steady_clock::time_point> last_message_time;
    last_message_time = std::chrono::steady_clock::now();
    std::thread keep_alive_th(keep_alive_checker, client_socket, client_id, std::ref(client_is_alive), std::ref(last_message_time));

    // Send questions one by one
    for (const auto& [question, options, answer_char] : quiz) {
        if (!client_is_alive) break;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - start).count();
        if (elapsed >= 5) break;

        // Prepend [CACHE] or [FRESH] tag to the question
        std::string cache_prefix = served_from_cache ? "[CACHE] " : "[FRESH] ";
        std::string q_text = "\033[2J\033[H" + cache_prefix + question + "\n";
        
        char option_label = 'A';
        for (const auto& opt : options) {
            // Trim leading whitespace from the option
            size_t first_char = opt.find_first_not_of(" \t");
            std::string trimmed_opt = (first_char == std::string::npos) ? opt : opt.substr(first_char);

            // Check if the option is already formatted (e.g., starts with "A)")
            if (trimmed_opt.length() >= 2 && isalpha(trimmed_opt[0]) && trimmed_opt[1] == ')') {
                q_text += trimmed_opt + "\n"; // If yes, use it directly
            } else {
                q_text += option_label;      // If no, add our own label
                q_text += ") ";
                q_text += opt + "\n";
            }
            option_label++;
        }
        q_text += "\nChoose the correct option: ";
        send(client_socket, q_text.c_str(), q_text.size(), 0);

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
                continue;
            } else {
                client_ans = received;
                break;
            }
        }

        if (!client_is_alive) break;

        std::string msg;
        if (!client_ans.empty() && toupper(client_ans[0]) == answer_char) {
            msg = "Correct Answer!\n\n";
            std::lock_guard<std::mutex> guard(leaderboard_mutex);
            leaderboard[client_id]++;
        } else {
            msg = "Wrong Answer! Correct answer is " + std::string(1, answer_char) + "\n\n";
        }
        send(client_socket, msg.c_str(), msg.size(), 0);

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
                break;
            }
        }
        if (!client_is_alive) break;

        if (choice == "L" || choice == "l") {
            std::string board;
            {
                std::lock_guard<std::mutex> lock(active_clients_mutex);
                board = getLeaderboard(client_id, active_clients);
            }
            send(client_socket, board.c_str(), board.size(), 0);
            
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
                    break;
                }
            }
        }
    }

    if (client_is_alive) {
        std::string end_msg = "Quiz over – final scores available\n";
        end_msg += getLeaderboard(client_id, active_clients);
        send(client_socket, end_msg.c_str(), end_msg.size(), 0);
    }
    
    client_is_alive = false;
    keep_alive_th.join();
    
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
    address.sin_port= htons(13033);

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address))<0){
        // std::cout<<"Binding failed\n";
        // return -1;
        perror("Binding failed");
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

