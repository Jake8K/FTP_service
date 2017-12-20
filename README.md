# FTP_service
### A Terminal Based File Transfer Application

## Server: ftserver.c
### To Compile:
- ```make``` or ```make ftserver``` (using the makefile provided)
- ```gcc -g ftserver.c -o ftserver -lpthread``` (on the command line)
- ```compilall``` (to use provided bash shell script)
### To Run:
 - ```ftserver <port number>```
 Server then runs indefinitely, servicing clients that connect until killed with ```SIGINT```
 ### To Remove Executable:
 - ```make clean```

## Client: ftclient.py
### To Run:
- ```[python3] ftclient.py <server> <server port> <command> [file name] [directory] [data port]```
    - ```<server>``` is the host where the server is running,
    - ```<server port>``` is the port where it's expecting connections,
    - ```<data port>``` is the port where the server can send the requested document or directory.
    - The client can send 3 commands to the server: ```-l``` (list directory) ```-g``` (get file) or ```cd``` (change directory).
       
- so, more specifically:
   - ```ftclient.py <server> <server port> -l <data port>```
   - ```ftclient.py <server> <server port> -g <file name> <data port>```
   - ```ftclient.py <server> <server port> cd <directory>```

Note: client will be prompted for username (admin) and password (password)... points for originality?

The program is able to transfer all kinds of files, tested up to 920 MB video file.


## NOTES:
The programs run better (user experience-wise, not functionality) when they're in different directories, I created a folder for the client program and its transfers.
- Server is multithreaded
   - The server will create a new thread for each new connecting client
- Requires a username and password to access the server
   - user name: admin
   - password:  password
- Client is able to change directories
- Program can handle various (all?) file types in addition to text.
  - tested with 920 MB .mp4 file, 88MB .Mv4 file, 8MB .pdf, 150KB aliceASCII.txt file, and 0MB .txt file

## Sources:
- OSU CS344 “Intro to Operating Systems” Lectures by Prof. Brewster
- OSU CS372 “Intro to Networking” Lectures by Prof. Redfield
- textbook: Computer Networking - A Top-Down Approach 6th Edition
- Official Python 3 Documentation
- Beej’s Guide
- Linux man pages
- code wiki
- Various stackoverflow posts



