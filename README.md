# Client_server_demo_project
**************************************************************************************************************************************************
Project to demonstrate client - server communication using TCP sockets.
After start, client application requests file name and IP address of the server (if they havn't been provided as arguments in the command line).
Then client establishes connection with the server and requests the file.
If file doesn't exist on the server, server sends the error message.
If file is found on the server, client receives the file.
In case the client already has file that the user requests, it sends update request to the server, containing file name and file size. When server receives the update request, it check if the file on the server has the same size.
If the file size is the same, server sends "No update" message. If file size is different, server sends the difference based on the number of bytes. Afdter receiving the update, client appends the data to the file on the client side. 
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

./client <"file name"> <"IP address in IPv4 format">

If <IP addrfess> is not provided as argument, server uses default IP address.

After receiving file client exits, server application still runs waiting for the next connection. Use Ctrl-C to exit.
**************************************************************************************************************************************************
Limitations
Applications run using default port 12345. Necessary to change according to the configuration.
Client application doesn't use multiple threads, needs to be modified to use POSIX threads. It will allow to initiate multiple connections with the server and request several files.
Server application uses POSIX threads to work with multiple connections, resource management is not implemented. For example, if two connections request the same file simultaneously, it may cause crash. Need to implement synchronization between diffewrent threads as well.
Update works correctly only for the case when size of the file on the client side is smaller, than on the server side. To allow the correct transfer for the update, every change on the file on the server side should be in the new line (otherwise it may cause formatting issues). Append to the file on the client side add updates on the new line of the client side file.
**************************************************************************************************************************************************
Future Improvements
Resolve formatting issues.
Modify client application to use POSIX threads.
**************************************************************************************************************************************************

License:
Theses programs are free software; you can redistribute them and/or modify iunder the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.


