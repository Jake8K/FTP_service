/*****************************************************************************************
 * Author: 	Jacob Karcz
 * Date: 	5.29.2017
 * Course:	CS372-400: Intro to Computer Networks
 * Program 2: 	ftserver.c - file transfer servce application
 * Description:	This is the server for a file transfer service application.
 * 		Runs a server process that waits for clients to request files or directory contents.
 *      Spins a new thread for each connecting client, services its directory change, 
 *      file transfer, or directory listing request through the client's data port (when
 *      approproate), and then closes the data connection.
 *      Able to transfer binary files including large files (tested to 950MB)
 *      Note: username & password for verification are admin/password
 ******************************************************************************************/



/* headers
 ------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* constants
 ------------*/
#define BUFFER_SIZE    2000
#define DEBUG          0

/* Functions
 ----------------------------*/
void exitError(const char *msg);
void error(const char *msg); 
int serverHandshake(int connectionFD, char* clientProcess, char* serverProcess);
int startup (int socketFD);
int openDataSocket (int dataPort, char* host);
void* ftpFunk(void*);
void ls(int dataSocket, char* host);
void sendFile(int dataPort, char* fileName, char* host);
int authenticate(char* authentication);


/*******************************************************************************************
 * Function: 		int main(int argc, char *argv[])
 * Description:		main function of the file transfer service server, allows clients to 
 *                  connect and services their requests on separate threads.
 * Parameters:		argv[0] is the program's name
 *                  argv[1] is the server's port number
 * Pre-Conditions: 	none
 * Post-Conditions: The server will run until closed with SIGINT, servicing clients requests.
 ********************************************************************************************/
int main(int argc, char *argv[]) {

	//variables
	int serverPort,
        serverSocket,
	    clientSocket;
    char host[1024],
         service[20];
    
    struct sockaddr_in serverAddress,
                       clientAddress;

	socklen_t sizeOfClientInfo;
    
    pthread_t clienThread;

	//check usage & args
	if (argc != 2) { fprintf(stderr,"USAGE: %s <port>\n", argv[0]); exit(1); }

    //set up listening socket
    serverPort = atoi(argv[1]);
    serverSocket = startup (serverPort);
    printf("File Transfer Server open on port %d\n", serverPort);
	listen(serverSocket, 5); // Flip the socket on - it can now receive up to 5 connections
    sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client
	//wait for clients
	while(1) {
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
        getnameinfo(&clientAddress, sizeof(clientAddress), host, sizeof(host), service, sizeof(service), 0);
        printf("connection received from %s\n", host);
        if (clientSocket < 0) error("ERROR on accept");
        if (pthread_create(&clienThread, NULL, ftpFunk, (void*)&clientSocket) < 0) //threadz! :)
            error("ERROR on thread creation"); //no threads :(
	}
	close(serverSocket); // Close the listening socket
	return 0; 
}
/*******************************************************************************************
 * Function: 		int startup( int serverPort)
 * Description:		Function to set up the server's listening (control) socket
 * Parameters:		The port number where the server will service clients
 * Pre-Conditions: 	None
 * Post-Conditions: The server's control socket is initialized and ready to start listening.
 * Returns:         The server socket, properly initialized.
 *******************************************************************************************/
int startup( int serverPort) {
    struct sockaddr_in serverAddress;
    int listenSocketFD;

    // Set up the address struct for this process (the server)
    memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    serverAddress.sin_family = AF_INET; // Create a network-capable socket
    serverAddress.sin_port = htons(serverPort); // Store the port number
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process
    
    // Set up the socket
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (listenSocketFD < 0) exitError("ERROR opening control/listening socket");
    
    //Bind the socket
    if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port or bust
        exitError("ERROR on binding control/listening socket");
    
    return listenSocketFD;
}

/*******************************************************************************************
 * Function:        int openDataSocket(int dataPort, char* host)
 * Description:		Function to set up the server's data transfer socket
 * Parameters:		The client's hostname and data transfer port
 * Pre-Conditions: 	The server and client are ready to exchange data through the socket
 * Post-Conditions: The server's data transfer socket is initialized and connected to the client
 * Returns:         The data socket, connected to the client process.
 ********************************************************************************************/
int openDataSocket(int dataPort, char* host) {
    struct sockaddr_in serverAddress;
    struct hostent *serverHost;
    int dataSocketFD;
    
    //prep for data socket
    memset((char *)&serverAddress, '\0', sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(dataPort);
    serverHost = gethostbyname(host);
    memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHost->h_addr, serverHost->h_length);
    
    // Set up data socket
    sleep(1); //bc python is slow
    dataSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocketFD < 0) error("ERROR opening data socket");
    
    // Connect data socket
    if (connect(dataSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) != 0)
        error("ERROR on connecting data socket");
    
    return dataSocketFD;
}


 /*******************************************************************************************
 * Function: 		void* ftpFunk(void* FD)
 * Description:		The thread's main (logic) function that handles client commands/requests
 * Parameters:		The sockaddr_in struct for the server/client control connection
 * Pre-Conditions: 	The client and server are connected and FD properly intitialized
 * Post-Conditions: The client has been serviced or turned away, when the function terminates 
 *                  so does the thread.
 ********************************************************************************************/
void* ftpFunk(void* FD) {
    //variables
    int i,
        serverSocket,
        controlSocket,
        dataSocketFT,
        serverPort,
        dataPort,
        charsRead,
        terminalLocation,
        homeDir,
        verifyConnect;
    
    char buffer[BUFFER_SIZE],
         commandLine[BUFFER_SIZE],
         newDir[200],
         fileName[200],
         client[50],
         clientIP[20];
    
    char* token = NULL; //bc evil strtok works...
    
    struct sockaddr_in serverAddress,
                       clientAddress;
    struct stat findFile;
    
    //clear buffers
    memset(commandLine, '\0', sizeof(commandLine));
    memset(newDir, '\0', sizeof(newDir));
    memset(buffer, '\0', sizeof(buffer));
    memset(fileName, '\0', sizeof(fileName));
    memset(client, '\0', sizeof(client));
    memset(clientIP, '\0', sizeof(clientIP));



    //fix the socket
    controlSocket = *(int*)FD;
    
    //get client data
    charsRead = recv(controlSocket, buffer, sizeof(buffer)-1, 0);
    if (charsRead == 0) {printf("No data received, closing connection.\n"); return NULL;}
    if (charsRead < 0)  {printf("ERROR: failed to receive client data\n"); return NULL;}
    
    //authenticate client
    if (authenticate(buffer)) {
        memset(buffer, '\0', sizeof(buffer));
        sprintf(buffer, "Authentication verified... Access granted");
        send(controlSocket, buffer, strlen(buffer), 0);
    }
    else {
        memset(buffer, '\0', sizeof(buffer));
        sprintf(buffer, "Verification failed: user name or password are incorrect");
        send(controlSocket, buffer, strlen(buffer), 0);
        return NULL;
    }
    
    memset(buffer, '\0', sizeof(buffer));
    charsRead = recv(controlSocket, buffer, sizeof(buffer)-1, 0);
    if (charsRead == 0) {printf("No data received, closing connection.\n"); return NULL;}
    if (charsRead < 0)  {printf("ERROR: failed to receive client data\n"); return NULL;}
    
    //parse out commandline
    strcpy(commandLine, buffer);
    
    token = strtok(commandLine, " \n\0");
    strcpy(client, token);
    token = strtok(NULL, " \n\0");
    strcpy(clientIP, token);
    printf("Servicing client %s\n", clientIP);
    token = strtok(NULL, " \n\0");
    
    if (strncmp(token, "cd", 2) == 0) {           //chdir
        token = strtok(NULL, " \n\0");
        //getcwd()?
        //if (token == NULL) { homeDir = getenv("HOME"); }
        strcpy(newDir, token);
        printf("Server directory change to \"%s\" requested\n", newDir);
        int movDir = chdir(newDir);
        if (movDir == -1) {
            perror("chdir error");
            memset(buffer, '\0', sizeof(buffer));
            sprintf(buffer, "ERROR: directory change failure");
            write(controlSocket, buffer, strlen(buffer));
        }
        else {
            printf("Directory changed successfully\n");
            memset(buffer, '\0', sizeof(buffer));
            sprintf(buffer, "Success! current directory: %s", newDir);
            write(controlSocket, buffer, strlen(buffer));
        }
        
        
    }
    else if (strcmp(token, "-l") == 0) {      //ls
        token = strtok(NULL, " \n\0");
        dataPort = atoi(token);
        printf("List directory requested on port %d\n", dataPort);
        ls(dataPort, clientIP);
    }
    else if (strcmp(token, "-g") == 0) {      //getFile
        token = strtok(NULL, " \n\0");
        strcpy(fileName, token);
        token = strtok(NULL, " \n\0");
        dataPort = atoi(token);
        printf("File \"%s\" requested on port %d\n", fileName, dataPort);
        if (stat(fileName, &findFile) < 0) {
            printf("failed to open/locate file, sending error message to %s:%d\n", clientIP, dataPort);
            memset(buffer, '\0', sizeof(buffer));
            sprintf(buffer, "ERROR: file not found, unable to open \"%s\"", fileName);
            send(controlSocket, buffer, strlen(buffer), 0);
            //write(controlSocket, buffer, strlen(buffer));

        }
        else {
            memset(buffer, '\0', sizeof(buffer));
            sprintf(buffer, "Transfering \"%s\"", fileName);
            write(controlSocket, buffer, strlen(buffer));
            sendFile(dataPort, fileName, clientIP);
        }
    }
    return NULL;
}

/*******************************************************************************************
 * Function:        void ls(int dataPort, char* host)
 * Description:		Function to send current directory contents to the client
 * Parameters:		The client's hostname and data transfer port
 * Pre-Conditions: 	The client is ready to receive the directory through the data socket
 * Post-Conditions: The server sent the requested directory contents to the client
 *                  and closes out the the data socket (created within this function)
 ********************************************************************************************/
void ls(int dataPort, char* host) {
    int dataSocket,
        bytesSent;
    char sendBuffer[BUFFER_SIZE];
    memset(sendBuffer, '\0', sizeof(sendBuffer));
    DIR* dir;
    struct dirent *dirContent;
    
    fflush(stdout);
    printf("Sending directory contents to %s:%d\n", host, dataPort);
    fflush(stdout);
    dataSocket = openDataSocket(dataPort, host);
    dir = opendir(".");
    if (dir != NULL) {
        while (dirContent = readdir(dir)) {
            strcat(sendBuffer, dirContent->d_name);
            strcat(sendBuffer, " ");
        }
        bytesSent = send(dataSocket, sendBuffer, strlen(sendBuffer), 0);
        if (bytesSent < 0) error("ERROR writing to socket");
        if (bytesSent < strlen(sendBuffer)) {
            send(dataSocket, &sendBuffer[bytesSent], strlen(sendBuffer) - bytesSent, 0);
        }
    }
    else error("ERROR: unable to open directory.");
    
    //close out
    closedir(dir);
    close(dataSocket);
}

/*******************************************************************************************
 * Function:        void sendFile(int dataPort, char* fileName, char* host)
 * Description:		Function to send the requested file to the client
 * Parameters:		The client's hostname, data transfer port, and file name of wanted file
 * Pre-Conditions: 	The client is ready to receive the file through the data socket
 * Post-Conditions: The server sent the requested file to the client and closes out the 
 *                  the data socket (created within this function)
 ********************************************************************************************/
void sendFile(int dataPort, char* fileName, char* host) {
    int dataSocket,
        fileSize,
        bytesRead,
        bytesWritten,
        bytesSent;
    int fileFD;
    char sendBuffer[BUFFER_SIZE];
    memset(sendBuffer, '\0', sizeof(sendBuffer));
    
    fflush(stdout);
    printf("Sending \"%s\" to %s:%d\n", fileName, host, dataPort);
    fflush(stdout);

    dataSocket = openDataSocket(dataPort, host);
    
    //open the file & copy it to buffer
    fileFD = open(fileName, O_RDONLY);
    if (fileFD < 1) {
        fprintf(stderr, "error opening %s\n", fileName);
        bytesWritten = send(dataSocket, "error, file not found or could not be opened\n", 45, 0);
        if (bytesWritten < 0) error("ERROR writing to socket");
        return;
    }
    fileSize = lseek(fileFD, 0, SEEK_END);
    if(DEBUG) { printf("file size: %d bytes\n", fileSize); }
    int pos = lseek(fileFD, 0, SEEK_SET);
    
    //"small" files to 1 MB
    if (fileSize < 1000000) {
        char fileBuffer[fileSize + 1];
        memset(fileBuffer, '\0', sizeof(fileBuffer));
        bytesRead = read(fileFD, fileBuffer, sizeof(fileBuffer)-1);
        if(DEBUG) { printf("file bytes read: %d bytes\n", bytesRead); }
        
        //send the file contents (using memcpy instead of strcpy so it can send binary)
        bytesSent = 0;
        if (fileSize < BUFFER_SIZE) {
            bytesWritten = send(dataSocket, fileBuffer, fileSize, 0);
            if (bytesWritten < 0) { error("ERROR writing to socket"); }
        }
        else {
            while (bytesSent <= fileSize){
                if ((fileSize - bytesSent) < BUFFER_SIZE) {
                    memset(sendBuffer, '\0', sizeof(sendBuffer));
                    memcpy(sendBuffer, &fileBuffer[bytesSent], fileSize - bytesSent);
                    bytesWritten = send(dataSocket, sendBuffer, fileSize - bytesSent, 0);
                    if (bytesWritten < 0) { error("ERROR writing to socket"); break; }
                    bytesSent += bytesWritten;
                    if(DEBUG){
                        printf("bytes sent(now): %d\nbytes sent(total): %d\n", bytesWritten, bytesSent);
                    }
                    break;
                }
                //part out the fileBuffer
                memset(sendBuffer, '\0', sizeof(sendBuffer));
                memcpy(sendBuffer, &fileBuffer[bytesSent], BUFFER_SIZE-1);
                
                //send
                bytesWritten = send(dataSocket, sendBuffer, BUFFER_SIZE-1, 0);
                if (bytesWritten < 0) { error("ERROR writing to socket"); break; }
                
                bytesSent += bytesWritten;
                //bytesSent += (BUFFER_SIZE - 1);
                if(DEBUG){
                    printf("bytes sent(now): %d\nbytes sent(total): %d\n", bytesWritten, bytesSent);
                }
            }
        }
        close(fileFD);
    }
    //"big" files to ?
    else {
        close(fileFD);
        size_t fileBytes;
        ssize_t chunkSize;
        char* fileChunk = NULL;
        FILE* file = fopen(fileName, "r");
        while ((chunkSize = getline(&fileChunk, &fileBytes, file)) != -1) {
            bytesSent = send(dataSocket, fileChunk, chunkSize, 0);
            if (bytesSent < 0) { error("ERROR writing to socket"); break; }
        }
        free(fileChunk);
        fclose(file);
    }
    close(dataSocket);

    
}


/*******************************************************************************************
 * Function:        int authenticate(char* authentication)
 * Description:		Function to verify the connecting client
 * Parameters:		A string containing the user name and password as a single string 
 *                  (no extra chars or white space between the user name and password)
 * Pre-Conditions: 	The string received should match the string bellow (in function)
 * Post-Conditions: Returns true (1) if the strings match, otherwise returns false (0)
 ********************************************************************************************/
int authenticate(char* authentication) {
    printf("Authenticating user... ");
    //authentication(username+password) must match the string bellow!
    if (strcmp(authentication, "adminpassword") != 0) {
        printf("\n... authentication failed.\n");
        return 0;
    }
    else {
        printf("\n... authentication verified.\n");
        return 1;
    }

}


/****************************************************
 * process connection verification function
 ***************************************************/
int serverHandshake(int connectionFD, char* clientProcess, char* serverProcess) {
	//variables
	int charsRead;
	char buffer[128];
		memset(buffer, '\0', sizeof(buffer));
	
	//read client ID
	charsRead = recv(connectionFD, buffer, sizeof(buffer)-1, 0); 
	if (charsRead < 0) { error("error reading client ID, terminating connection."); return 2;}

	//send server ID
	send(connectionFD, serverProcess, strlen(serverProcess), 0);

	
	//verify ID
	if(strncmp(buffer, clientProcess, charsRead) == 0) {
		return 0; //ID verified
	}
	else {
		return 1; //wrong ID
	}
}


/****************************************************
 * SIGCHILD signal handler function
 ***************************************************/
void zombies() {
	int exitStat;
	pid_t zombieChild;

	zombieChild = waitpid(-1, &exitStat, WNOHANG);
}


/************************************************************
 * Error Reporting Functions
 ************************************************************/
void exitError(const char *msg) { perror(msg); exit(1); }
void error(const char *msg) { perror(msg); }
