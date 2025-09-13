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

void handle_client(int client_socket, int client_id){
    char buffer[1024] = {0};

    // Ask for genre
    std::string prompt = "Enter quiz genre:\n";
    send(client_socket, prompt.c_str(), prompt.size(), 0);

    // Receive genre
    memset(buffer, 0, sizeof(buffer));
    int bytes_read = read(client_socket, buffer, sizeof(buffer));
    if (bytes_read <= 0) {
        std::cout << "Client disconnected before genre: " << client_socket << "\n";
        close(client_socket);
        return;
    }
    std::string genre(buffer);
    std::cout << "[DEBUG] Received genre: " << genre << std::endl;

    // Get quiz from LLM (as JSON string)
    std::string quiz_response = query_llm("es23btech11033", genre);
    std::cout << "[DEBUG] LLM raw response: " << quiz_response << std::endl;

    // Extract the quiz text from the JSON response
    std::string quiz_text = extract_content_field(quiz_response);
    std::cout << "[DEBUG] Extracted quiz text:\n" << quiz_text << std::endl;
    auto quiz = parse_json_quiz(quiz_text);
    std::cout << "[DEBUG] Parsed quiz size: " << quiz.size() << std::endl;
    if (quiz.empty()) {
        std::string error_msg = "Error generating quiz. Please try again.\n";
        send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        close(client_socket);
        return;
    }

    // Send questions one by one
    for (const auto& [question, options, answer] : quiz) {
        std::string q_text = question + "\n";
        for (const auto& opt : options) q_text += opt + "\n";
        send(client_socket, q_text.c_str(), q_text.size(), 0);

        // Receive answer
        memset(buffer, 0, sizeof(buffer));
        int ans_bytes = read(client_socket, buffer, sizeof(buffer));
        if (ans_bytes <= 0) break;
        std::string client_ans(buffer);

        // Feedback
        std::string msg;
        if (!client_ans.empty() && toupper(client_ans[0]) == answer) {
            msg = "Correct Answer!\n";
            std::lock_guard<std::mutex> guard(leaderboard_mutex);
            leaderboard[client_id]++;
        } else {
            msg = std::string("Wrong Answer! Correct answer is ") + answer + "\n";
        }
        send(client_socket, msg.c_str(), msg.size(), 0);

        sleep(3); // Wait for 3 seconds before next question
    }

    close(client_socket);
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
        std::cout<<"Client connected :"<<new_socket<<"\n";
        std::thread t(handle_client,new_socket,client_id++);
        t.detach(); // Detach to run thread for each client
    }

    return 0;
}