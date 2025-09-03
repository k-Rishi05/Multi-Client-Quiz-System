- Wrote a server.cpp and client.cpp

client.cpp - sends a genre or requests leaderboard
server.cpp - hardcoded a question which is sent to client by ignoring the genre. if leaderboard requested prints it
client.cpp - send an answer back to server
server.cpp - checks the answer and increases points if right

- print leaderboard and increse points are guarded by mutex to avoid race conditions critical sections
- multiple clients are handled by thread library. each thread looks after one client. Done by DETACH.

TODOs

- Remove the hardcoding part and see how to integrate LLM when API key is given. (IG Prompt Engineering)
- send genre to LLM get question with options and right answer. Need to store it somewhere to verify the answer given by user. maybe each quesion will have QUESTION_ID
- Check what is asked to do in assignment (PART-1) and also beautify Leaderboard a bit and also zeros are not getting printed.
