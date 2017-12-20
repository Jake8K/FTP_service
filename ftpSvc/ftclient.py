#!/usr/bin/python

# Author: Jacob Karcz
# Date: 05.25.2017
# Course: CS372-400: Intro to Computer Networks
# File Name: ftclient.py
# Description: A File Transfer Service Client Application
#              Connects to the server running on the specified control port and has
#              its requests fulfilled on the specified data port.
#---------------------------------------------------------------------------------------#

# get the necessary libraries
from socket import *
import sys
import threading
import os.path
import fileinput
import fcntl
import string

# command line argument validation function
# input: command line arguments
# output: if anyb incorrect argument, prints helpful information and exits
def checkUsage():
    if (len(sys.argv) < 5 or len(sys.argv) > 6):
        print("usage: [python3] ftclient.py <host> <ctrl port> <-g> <filename> <data port>")
        print("       [python3] ftclient.py <host> <ctrl port> <-l> <data port>")
        print("       [python3] ftclient.py <host> <ctrl port> <cd> <path>")
        exit(1)
    elif (int(sys.argv[2]) < 1024 or int(sys.argv[2]) > 65535):
        print("control port invalid")
        exit(1)
    elif (sys.argv[3] == "-l" and (int(sys.argv[4]) > 65535 or int(sys.argv[4]) < 1024)):
        print("data port invalid")
        exit(1)
    elif (len(sys.argv) == 6 and (int(sys.argv[5]) > 65535 or int(sys.argv[5]) < 1024)):
        print("data port invalid")
        exit(1)
    elif (sys.argv[3] != "-l" and sys.argv[3] != "-g" and sys.argv[3] != "cd"):
        print("command {0} not recognized".format(sys.argv[3]))
        exit(1)

# function to connect to the server on the control connection
# input: none
# output: client's socket, connected to the server
def ServerConnect():
    serverName = sys.argv[1]
    serverPort = int(sys.argv[2])
    clientSocket = socket(AF_INET, SOCK_STREAM)
    clientSocket.connect((serverName, serverPort))
    return clientSocket

# function to receive data connection from the server
# input: the client's data port
# output: the data connection socket, connected to the server
def dataSocketSetup(dataPort):
    dataSocket = socket(AF_INET, SOCK_STREAM)
    dataSocket.bind(('', dataPort))
    dataSocket.listen(1)
    connection, address = dataSocket.accept()
    return connection

# function to send the request/command to the server
# input: the control socket (connected to server)
# output: The server should receive the command line (request)
def makeRequest(serverSocket):
    clientIP = myIP()
    if len(sys.argv) == 6:
        commandLine = sys.argv[1] + " " + clientIP + " " + sys.argv[3] + " " + sys.argv[4] + " " + sys.argv[5]
    else:
        commandLine = sys.argv[1] + " " + clientIP + " " + sys.argv[3] + " " + sys.argv[4]
    serverSocket.send(commandLine.encode("utf-8"))

# function to receive a file from the server
# input: the data socket (connected to the server)
#        the name of the file (locally)
# output: The contents of the file received, named fileName
#         have been transferred to the current directory.
def receiveFile(dataSocket, fileName):
    #connection, address = dataSocket.accept()
    
    if ".txt" in fileName:
        file = open(fileName, "w")
        packet = dataSocket.recv(10000)
        while (packet):
            buf = packet.decode("utf-8")
            file.write(buf)
            packet = dataSocket.recv(10000)
            if not packet: break
            if len(packet) == 0: break
        file.close()
    else:
        file = open(fileName, "wb")
        packet = dataSocket.recv(10000)
        while (packet):
            file.write(packet)
            packet = dataSocket.recv(10000)
            if not packet: break
            if len(packet) == 0: break
        file.close()

# function to receive a directory listing from the server
# input: the data socket (connected to the server)
# output: The contents of the directory printed to the terminal
def receiveDir(dataSocket):
    #connection, address = dataSocket.accept()
    dirData = dataSocket.recv(10000)
    directory = dirData.split()
    for fileName in directory:
        print(fileName.decode("utf-8"))

# function to send a username and password to the server
# input: the control socket (connected to the server)
# output: The server authorizes the user or it doesn't and
#         the function closes the control connection and exits
def authenticate(serverSocket):
    user = input("User Name: ")
    password = input("Password: ")
    auth = user + password
    serverSocket.send(auth.encode("utf-8"))
    reply = serverSocket.recv(88)
    replyString = reply.decode("utf-8")
    print (replyString)
    if replyString[:1] == "V":
        serverSocket.close()
        sys.exit(0)

# function to get client host's IP (to send to the server)
# input: none
# output: The IP address of the current host
def myIP():
    s = socket(AF_INET, SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    return s.getsockname()[0]

# main function for the file transfer service client
# input: command line arguments ( see checkusage() )
# output: assuming the appropriate command line arguments and password,
#         the server has serviced the client and transfered any requested data or
#         directory listings, or change of directory.
if __name__ == "__main__":
    checkUsage()
    
    Ssocket = ServerConnect()
    
    authenticate(Ssocket)
    makeRequest(Ssocket)
    
    if sys.argv[3] == "-l":
        dataPort = int(sys.argv[4])
        dataSocket = dataSocketSetup(dataPort)
        print("Receiving directory structure from {0}:{1}".format(sys.argv[1], dataPort))
        receiveDir(dataSocket)
        dataSocket.close()
    
    elif sys.argv[3] == "-g":
        fileName = sys.argv[4]
        dataPort = int(sys.argv[5])
        print("Reqesting file transfer: {0} from {1}:{2}".format(fileName, sys.argv[1], dataPort))
        fileStat = Ssocket.recv(100)
        print("message from {0}:{1}: {2}".format(sys.argv[1], sys.argv[2], fileStat.decode("utf-8")))
        if "ERROR" not in fileStat.decode("utf-8"):
            dataSocket = dataSocketSetup(dataPort)
            if os.path.exists(fileName):
                overwrite = input("File already exists locally. \nWould you like to overwrite (O), cancel (C), or create a new name (N)? O/C/N\n Selection: ")
                if overwrite[:1] == "c" or overwrite[:1] == "C":
                    print("Cancelling and exiting the program. Goodbye.")
                    Ssocket.close()
                    dataSocket.close()
                    sys.exit(0)
                elif overwrite[:1] == "n" or overwrite[:1] == "N":
                    newName = input("New file name: ")
                    receiveFile(dataSocket, newName)
                elif overwrite[:1] == "o" or overwrite[:1] == "O":
                    receiveFile(dataSocket, fileName)
            else:
                receiveFile(dataSocket, fileName)
            print("File transfer complete")
            dataSocket.close()

    elif sys.argv[3] == "cd":
        newDir = sys.argv[4]
        msg = Ssocket.recv(50)
        print(msg.decode("utf-8"));

    Ssocket.close()









