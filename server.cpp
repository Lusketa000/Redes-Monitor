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

atomic<bool> keepRunningClient(false);

string getCPUUsage(pid_t pid) {
    // TODO: implementar a leitura real de CPU do processo
    return "CPU usada: 50.00 %\n";
}

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

void output(int clientSocket, pid_t pid, int tempo, int tipo, atomic<bool>& keepRunning) {
    while (keepRunning) {
        string result = (tipo == 1) ? getMemoryUsage(pid) : getCPUUsage(pid);
        send(clientSocket, result.c_str(), result.size(), 0);
        this_thread::sleep_for(chrono::seconds(tempo));
    }
}

void input(int clientSocket, pid_t pid) {
    char buffer[1024];
    int bytesReceived;

    thread A;
    thread B;

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';

        cout << "Comando do cliente " << clientSocket << ": " << buffer;

        string cmd;
        int Tempo = 1;
        istringstream iss(buffer);
        iss >> cmd >> Tempo;

        if (cmd == "MEM" || cmd == "MEM\n") {
            if (!keepRunningClient) {
                keepRunningClient = true;
                if (A.joinable()) A.join();
                A = thread(output, clientSocket, pid, Tempo, 1, ref(keepRunningClient));
            }
        } else if (cmd == "CPU" || cmd == "CPU\n") {
            if (!keepRunningClient) {
                keepRunningClient = true;
                if (B.joinable()) B.join();
                B = thread(output, clientSocket, pid, Tempo, 0, ref(keepRunningClient));
            }
        } else if (cmd == "EXIT" || cmd == "EXIT\n") {
            keepRunningClient = false;

        } else {
            string msg = "Comando desconhecido\n";
            send(clientSocket, msg.c_str(), msg.size(), 0);
        }

        memset(buffer, 0, sizeof(buffer));
    }

    cout << "Cliente " << clientSocket << " desconectou.\n";
    keepRunningClient = false;

    if (A.joinable()) {
        A.join();
    }

        if (B.joinable()) {
        B.join();
    }


    close(clientSocket);
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
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
        if (clientSocket < 0) {
            cerr << "Erro ao aceitar conexão.\n";
            continue;
        }

        cout << "Cliente conectado! Socket: " << clientSocket << endl;

        const char* welcomeMsg = "Bem-vindo ao servidor!\n";
        send(clientSocket, welcomeMsg, strlen(welcomeMsg), 0);

        threads.emplace_back(thread(input, clientSocket, pid));
    }

    close(serverSocket);
    return 0;
}
