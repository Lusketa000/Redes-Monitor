// C++ program to show the example of server application in
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <tuple>
#include <vector>

using namespace std;

void input(int clientSocket) {
    cout << "funcao input" << endl;
    char buffer[1024];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesReceived] = '\0'; // Garante terminação
        cout << "Message from client " << clientSocket << ": " << buffer << endl;
        memset(buffer, 0, sizeof(buffer)); // Limpa o buffer
    }
}

void output(int clientSocket) {
    while(1) {
        // sending data
        const char* message = "Hello, client!";
        send(clientSocket, message, strlen(message), 0);

        this_thread::sleep_for(chrono::seconds(5));
    }

}

int main()
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    vector<tuple<thread, thread>> threads;
    //vector<int> clientSockets;

    while (1) {
        // accepting connection request
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        //clientSockets.push_back(clientSocket);

        cout << "conectou um cliente!!!!!!11" << endl;
        
        // sending data
        const char* message = "Oi cliente";
        send(clientSocket, message, strlen(message), 0);

        threads.emplace_back(
            thread(input, clientSocket), 
            thread(output, clientSocket)
        );
    }
        
    // TODO: Consertar pq senao nunca vai chegar aq
    // closing the socket.
    //close(serverSocket);

    return 0;
}