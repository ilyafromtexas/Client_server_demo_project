# Client_server_demo_project
**************************************************************************************************************************************************
Project to demonstrate client - server communication using TCP sockets.
After start, client application requests file name and IP address of the server (if they havn't been provided as arguments in the command line).
Then client establishes connection with the server and requests the file.
If file doesn't exist on the server, server sends the error message.
If file is found on the server, client receives the file.
Server code is written to support multiple sockets using POSIX thread approach.
***************************************************************************************************************************************************

How to compile and use server and client application
Client:
gcc client.c -o client

Server:
gcc server.c -o server -lpthread

Usage:
Run the server application:
./server

Run the client application:
./client

It is possible to provide file name and server IP as arguments for the client application launch:
./client <file name> <IP address in IPv4 format>

After receiving file client exits, server application still runs waiting for the next connection. Use Ctrl-C to exit.
**************************************************************************************************************************************************
Limitations:
Applications run using default port 12345. Necessary to change according to the configuration.
Client application doesn't use multiple threads, needs to be modified to use POSIX threads. It will allow to initiate multiple connections with the server and request several files.
Server application uses POSIX threads to work with multiple connections, resource management is not implemented. For example, if two connections request the same file simultaneously, it may cause crash. Need to implement synchronization between diffewrent threads as well.
**************************************************************************************************************************************************
Future Improvements
Check if the file has been already sent.
If the client has older version of the file, check the differences and transfer from the server only appended data to add to the existing file on the client side.
Modify client application to use POSIX threads.
**************************************************************************************************************************************************

License:
Theses programs are free software; you can redistribute them and/or modify iunder the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.


