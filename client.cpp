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
    char buffer[1024];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesReceived] = '\0'; // Garante terminação
        cout << "\r                                        \r";
        cout << buffer;
        fflush(stdout);
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
    char buffer[2048] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << buffer;

    thread thread(input, clientSocket);

    while(true) {
        cout << "> ";
        fflush(stdout);

        string message;
        getline(cin, message);

        if(!message.empty()) {
            send(clientSocket, message.c_str(), message.size(), 0);
        }

        string temp_msg = message;
        for(auto &c : temp_msg) c = toupper(c);
        if(temp_msg == "EXIT") {
            break;
        }
        
    }

    cout << "Encerrando a conexão!!" << endl;

    shutdown(clientSocket, SHUT_WR);

    // closing socket
    thread.join();
    close(clientSocket);

    return 0;
}