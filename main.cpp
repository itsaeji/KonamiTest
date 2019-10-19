/*
 * main.cpp
 *
 *  Created on: Aug 21, 2019
 *      Author: Anthony Garcia aka Trip
 *     Website: https://www.tripdevs.com
*/
#include <time.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tinyxml2.h"
#include <fstream>
#include <thread>

pthread_mutex_t dataLock = PTHREAD_MUTEX_INITIALIZER;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

tinyxml2::XMLDocument * build_response_doc(std::string command)
{
	//Declare all elements of the response.
    time_t current_time = time(NULL);
	tinyxml2::XMLDocument * responseDoc = new tinyxml2::XMLDocument();
	tinyxml2::XMLElement * responseElement;
	tinyxml2::XMLElement * commandElement;
	tinyxml2::XMLElement * statusElement;
	tinyxml2::XMLElement * timeElement;


	responseDoc->NewDeclaration();
	responseElement = responseDoc->NewElement("response");
	commandElement = responseDoc->NewElement("command");
	statusElement = responseDoc->NewElement("status");
	timeElement = responseDoc->NewElement("time");
	responseDoc->InsertFirstChild(responseElement);
	responseElement->InsertFirstChild(commandElement);
	commandElement->SetText(command.c_str());
	responseElement->InsertAfterChild(commandElement, statusElement);
	statusElement->SetText("complete");
	responseElement->InsertAfterChild(statusElement, timeElement);
	timeElement->SetText(ctime(&current_time));

	return responseDoc;

}

void xmlPrint(tinyxml2::XMLNode * dataToPrint)
{
	tinyxml2::XMLElement * iterator = dataToPrint->FirstChildElement("row");
	while(iterator != NULL)
	{
		std::cout << iterator->FirstAttribute()->Value();
		std::cout << ": ";
		std::cout << iterator->GetText() << std::endl;
		iterator = iterator->NextSiblingElement("row");
	}
}

void xmlAdd(tinyxml2::XMLNode * dataToAdd)
{
	int result = 0;
	tinyxml2::XMLElement * iterator = dataToAdd->FirstChildElement("row");
	while(iterator != NULL)
	{
		result += atoi(iterator->GetText());
		iterator = iterator->NextSiblingElement("row");
	}
	std::cout << "Result of addition: "<< result << std::endl;
}

void xmlSub(tinyxml2::XMLNode * dataToSub)
{
	int result = 0;
	tinyxml2::XMLElement * iterator = dataToSub->FirstChildElement("row");
	result = atoi(iterator->GetText());
	iterator = iterator->NextSiblingElement("row");
	while(iterator != NULL)
	{
		result -= atoi(iterator->GetText());
		iterator = iterator->NextSiblingElement("row");
	}
	std::cout << "Result of subtraction: "<< result << std::endl;
}

void * socketThread(void * arg)
{
	int newSocket = *((int *)arg);

	// Declare document and printer to parse all incoming and outgoing XML documents
	tinyxml2::XMLDocument testDoc;
	tinyxml2::XMLPrinter printer;
	tinyxml2::XMLDocument * outputDoc;

	// Declare a buffer to receive the stream of data from the client socket
	char buffer[256];

	// Clean any extraneous data using the bzero function
	bzero(buffer,256);

	// Reads the data from the socket into the buffer character array.
	int n = read(newSocket,buffer,255);

	// Locks the data in this thread to
	// prevent any other threads from accessing
	// data in this thread.
	pthread_mutex_lock(&dataLock);

	// Using TinyXML2, parses the data from the
	// buffer into an XML style document.
	testDoc.Parse(buffer);

	// Declares and defines a string to use as a comparator
	// for reading commands and elements
	std::string requestString = testDoc.FirstChildElement("request")->Name();

	// Does a comparison to determine if the socket produced an error or
	// if the data sent via the socket is indeed an XML packet
	if(n < 0 && requestString != "request")
	{
		// Notify the client that the data sent is not XML.
		send(newSocket, "Please only send XML.", 15, 0);

		// Unlocks the thread
		pthread_mutex_unlock(&dataLock);

		// Closes the connection with the client.
		close(newSocket);

		// Closes the thread to rejoin the main program.
		pthread_exit(NULL);
	}
	else
	{
		// Determines if the element of request is indeed a command.
		std::string commandString = testDoc.FirstChildElement("request")->FirstChildElement("command")->Name();
		if (commandString == "command")
		{
			size_t stringSize = 0;
			// Determine what command is being requested.
			std::string stringToUse = testDoc.FirstChildElement("request")->FirstChildElement("command")->GetText();
			if(stringToUse == "Print")
			{
				xmlPrint(testDoc.FirstChildElement("request")->FirstChildElement("command")->NextSibling());
				outputDoc = build_response_doc(stringToUse);
				outputDoc->Print(&printer);
				stringSize = sizeof(printer.CStr()) * 16;
				send(newSocket, printer.CStr(), stringSize, 0);

			}
			else if(stringToUse == "Add")
			{
				xmlAdd(testDoc.FirstChildElement("request")->FirstChildElement("command")->NextSibling());
				outputDoc = build_response_doc(stringToUse);
				outputDoc->Print(&printer);
				stringSize = sizeof(printer.CStr()) * 16;
				send(newSocket, printer.CStr(), stringSize, 0);

			}
			else if(stringToUse == "Sub")
			{
				xmlSub(testDoc.FirstChildElement("request")->FirstChildElement("command")->NextSibling());
				outputDoc = build_response_doc(stringToUse);
				outputDoc->Print(&printer);
				stringSize = sizeof(printer.CStr()) * 16;
				send(newSocket, printer.CStr(), stringSize, 0);
			}
			else
			{
				send(newSocket, "\nUnknown Command\n", 16, 0);
			}
		}





	}
	pthread_mutex_unlock(&dataLock);
	printf("\nHere is the message: %s\n",buffer);
	close(newSocket);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int masterSocket, newSocket, portNumber;
    int i = 0;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    // create a socket
    // socket(int domain, int type, int protocol)
	masterSocket =  socket(AF_INET, SOCK_STREAM, 0);
	if (masterSocket < 0)
	{
		error("ERROR opening socket");
	}
	// clear address structure
	bzero((char *) &serv_addr, sizeof(serv_addr));

	// passes commandline arguments for ip address and port number to use
	switch(argc)
	{
	case 2:
		fprintf(stderr,"Only ip address provided. Server will bind to default port 5000\n");
		serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
		portNumber = atoi("5000");
		break;
	case 3:
		serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
		portNumber = atoi(argv[2]);
		break;
	default:
		fprintf(stderr,"No port or ip address provided. Server will default to localhost:5000\n");
		serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		portNumber = atoi("5000");
		break;
	}
	/* setup the host_addr structure for use in bind call */
	// server byte order
	serv_addr.sin_family = AF_INET;

	// convert short integer value for port must be converted into network byte order
	serv_addr.sin_port = htons(portNumber);

	// bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
	// bind() passes file descriptor, the address structure,
	// and the length of the address structure
	// This bind() call will bind  the socket to the current IP address on port, portno
	if (bind(masterSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		error("ERROR on binding");
	}

	// This listen() call tells the socket to listen to the incoming connections.
	// The listen() function places all incoming connection into a backlog queue
	// until accept() call accepts the connection.
	// Here, we set the maximum size for the backlog queue to 50.
	listen(masterSocket,50);

	// Declare an array of threads to work on
	pthread_t tid[60];

	// The accept() call actually accepts an incoming connection
	clilen = sizeof(cli_addr);
	// This accept() function will write the connecting client's address info
	// into the the address structure and the size of that structure is clilen.
	// The accept() returns a new socket file descriptor for the accepted connection.
	// So, the original socket file descriptor can continue to be used
	// for accepting new connections while the new socker file descriptor is used for
	// communicating with the connected client.
	// bool to keep the loop active, breaks if client sends "stop" message.
	bool keepGoing = true;
	while(keepGoing)
	{
			// Accept connection from client socket.
		newSocket = accept(masterSocket,(struct sockaddr *) &cli_addr, &clilen);

		// create a new thread to work on for multiple, simultaneous clients
		pthread_create(&tid[i], NULL, socketThread, &newSocket);

		// Throw an error if something goes wrong.
		if (newSocket < 0)
		{
			error("ERROR on accept");
		}

		printf("server: got connection from %s port %d\n",inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

		if( i >= 50)
		{
			i = 0;
			while(i < 50)
			{
				pthread_join(tid[i++], NULL);
			}
		}
	}
    return 0;
}


