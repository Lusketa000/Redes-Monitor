// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

using namespace std;

void input(int clientSocket) {
    cout << "funcao input" << endl;
    char buffer[1024];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesReceived] = '\0'; // Garante terminação
        cout << "Message from servidor " << clientSocket << ": " << buffer << endl;
        memset(buffer, 0, sizeof(buffer)); // Limpa o buffer
    }
}

int main()
{
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress,
            sizeof(serverAddress));

    // recieving data
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Message from servidor: " << buffer << endl;

    thread thread(input, clientSocket);

    while(1) {
        string message;
        getline(cin, message);
        send(clientSocket, message.c_str(), message.size(), 0);
        
    }

    // closing socket
    close(clientSocket);

    return 0;
}