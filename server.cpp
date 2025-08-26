#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sstream>
#include <cstring>

using namespace std;

void output(int clientSocket, pid_t pid, int tempo);
void input(int clientSocket, pid_t pid);

// Flag de controle global (output QUIT)
atomic<bool> keepRunning(false);

// Função para ler memória de um PID
string getMemoryUsage(pid_t pid) {
    ifstream statusFile("/proc/" + to_string(pid) + "/status");
    if (!statusFile) return "Processo não encontrado\n";

    string line;
    long mem_kb = 0;
    while (getline(statusFile, line)) {
        if (line.rfind("VmRSS:", 0) == 0) { 
            mem_kb = stol(line.substr(6));
            break;
        }
    }
    return "Memória usada: " + to_string(mem_kb) + " kB\n";
}

// Thread de entrada (input) que lê comandos do cliente
void input(int clientSocket, pid_t pid) {
    char buffer[1024];
    int bytesReceived;
    thread outputThread;

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesReceived] = '\0'; // garante terminação

        cout << "Comando do cliente " << clientSocket << ": " << buffer << endl;

        string cmd;
        int Tempo = 1;

        istringstream iss(buffer);

        iss >> cmd;
        iss >> Tempo;

        if (cmd == "MEM\n" || cmd == "MEM") {
            if (!keepRunning) {
                keepRunning = true;
                outputThread = thread(output, clientSocket, pid, Tempo); // envia memória a cada X segundos
                outputThread.detach();
            }
        } else if (cmd == "QUIT\n" || cmd == "QUIT") {
            keepRunning = false;
        } else {
            string msg = "Comando desconhecido\n";
            send(clientSocket, msg.c_str(), msg.size(), 0);
        }

        memset(buffer, 0, sizeof(buffer));
    }

    cout << "Cliente " << clientSocket << " desconectou.\n";
    keepRunning = false;
    close(clientSocket);
}

// Thread de saída (output) que envia info para o Client
void output(int clientSocket, pid_t pid, int tempo) {
    while (keepRunning) {
        string mem = getMemoryUsage(pid);
        send(clientSocket, mem.c_str(), mem.size(), tempo);
        this_thread::sleep_for(chrono::seconds(tempo));
    }
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    listen(serverSocket, 5);

    vector<thread> threads;
    pid_t pid = getpid();

    cout << "Servidor aguardando conexão...\n";

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        cout << "Cliente conectado! Socket: " << clientSocket << endl;

        const char* welcomeMsg = "Bem-vindo ao servidor!\n";
        send(clientSocket, welcomeMsg, strlen(welcomeMsg), 0);

        threads.emplace_back(thread(input, clientSocket, pid));
    }

    close(serverSocket);
    return 0;
}
