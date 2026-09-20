# Quiz Game: Client-Server Application
 
## System Requirements
* **Operating System:** Tested on macOS Sequoia (Version 15.3.1)
---
 
## Compilation & Execution
 
The project is divided into two parts:
 
### Part 1(A): Without Cache
 
#### Server Side
```bash
# To Compile
g++ -std=c++17 server.cpp -o server -pthread
 
# To Run
./server
```
 
#### Client Side
```bash
# To Compile
g++ -std=c++14 client.cpp -o client
 
# To Run
./client
```
 
---
 
### Part B: With Cache
 
#### Server Side
```bash
# To Compile
g++ -std=c++17 server_cache.cpp -o server -pthread
 
# To Run
./server
```
 
#### Client Side
```bash
# To Compile
g++ -std=c++14 client_cache.cpp -o client
 
# To Run
./client
```
 
---
 
## Traffic Analysis Commands
 
### Command to capture `.pcap` files
```bash
tcpdump -i en0 -s 0 -w client_1A_N1.pcap 'host 192.168.50.111 and port 13033'
```
 
### Command to calculate Throughput
```bash
tshark -r client_1A_N1.pcap -q -z io,stat,0
```
 
---
 
## Configuration Details
 
* **Server Port:** The server listens for connections on port `13033`.
* **Quiz Length:** Each client will receive a total of 10 questions.
---
 
## Sample I/O
 
### Server Output Sample
The server logs connection events, LLM interactions, and client cleanup.
 
```text
Socket created successfully
Binding successful
Listening
LLM Reserve Response: HTTP/1.1 200 OK...
Client connected: 4 with ID: 1
[DEBUG] Received genre: Science
[DEBUG] LLM raw response: {"content":"{\"questions\":[...]}"}
[DEBUG] Extracted quiz text: {"questions":[...]}
Client connected: 5 with ID: 2
[INFO] Client 1 session finished and resources cleaned up.
```
 
### Client Output Sample
The client provides an interactive quiz experience, including real-time leaderboards.
 
```text
Connected to server
Enter quiz genre:
Science
 
# Screen Clears #
 
What is the chemical symbol for Gold?
A) Go
B) Gd
C) Au
D) Ag
 
Choose the correct option: C
Correct Answer!
 
Press [L] for Leaderboard, or [Enter] for Next Question: L
 
+-----------------------------------+
|         ACTIVE LEADERBOARD        |
+-----------+-----------------------+
| Player ID | Score                 |
+-----------+-----------------------+
| 1         | 1              <-- YOU|
| 2         | 10                    |
+-----------+-----------------------+
--- Leaderboard Displayed ---
 
Press [Enter] to continue...
```
 
---
 
## Keep-Alive Mechanism
 
To maintain a stable connection and handle timeouts, the server and client use a keep-alive protocol that runs in the background:
 
* **Ping:** Every 5 seconds, the server sends a `KEEP_ALIVE` message to the client.
* **Pong:** The client automatically responds with `ALIVE_OK` without interrupting the user.
* **Timeout:** If the server doesn't receive any message from a client for over 10 seconds, it considers the client disconnected and terminates the connection.
