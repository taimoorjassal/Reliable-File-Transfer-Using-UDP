# Reliable File Transfer Using UDP
 
We have used C language to develope a reliable file transfer system.

We have used segments with sequence numbers to get reliable file transfer

UDP protocol is used to reach TCP like reliability.


**Code to compile Server code:
"gcc server.c -oserver -lpthread"

Code to execute Server code:
"./server 9800"
can use any port number instead of 9800

Code to compile Client Code:
"gcc client.c -oclient -lpthread"

Code to execute client code:
"./client 9800"
can use any port number instead of 9800

The file names can be changed in the start of each code file in the "#DEFINE" part
The number of packets in segment can be cahnged in the "#DEFINE" part
The size of each packet can be changed in the "#DEFINE" part.**
