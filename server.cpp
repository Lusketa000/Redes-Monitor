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
#include <ctime>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <cstdio>
#include <algorithm>

using namespace std;

struct monitorControl {
    std::mutex mtx;      
    std::condition_variable cv;   
    std::atomic<bool> keepRunning{false};
};

void sendMenu(int clientSocket) {
    std::stringstream ss;
    ss << "------------------- MENU ------------------\n";
    ss << "Uso de CPU (%): CPU <intervalo>\n";
    ss << "Uso de Memória (kB): MEM <intervalo>\n";
    ss << "Terminar Monitores: Quit <CPU | MEM | ALL>\n";
    ss << "Sair: Exit\n";
    ss << "--------------------------------------------\n";
    std::string menuMsg = ss.str();
    send(clientSocket, menuMsg.c_str(), menuMsg.size(), 0);
}

string getCPUUsage() {

    //Comando "top -b -n 1 | grep '^%Cpu(s)'" para pegar porcentagem de uso de CPU
    //Vai retornar: %Cpu(s):  1.5 us,  0.8 sy,  0.0 ni, 97.5 id, ...
    //Usamos o "id", que mostra o a porcentagem em "idle" da CPU
    const char* command = "top -b -n 1 | grep Cpu";
    char buffer[256];
    string result = "CPU usada: 0.00 %\n";

    FILE* file = popen(command, "r");

    if(fgets(buffer,sizeof(buffer), file) != nullptr) {
        string line = buffer;
        size_t position = line.find("id,");

        //lógica para achar o "id" no meio do output do comando
        if(position != string::npos) {
            size_t start_position = line.rfind(',', position);

            if(start_position != string::npos) {
                string idle_usage = line.substr(start_position + 1, position - (start_position + 1));
                
                //Evita que o separador decimal seja uma vírgula, para que o stod funcione corretamente
                replace(idle_usage.begin(), idle_usage.end(), ',', '.');

                //Cálculo para a porcentagem de uso da CPU
                double idle_percentage = stod(idle_usage);
                double cpu_percentage = 100.0 - idle_percentage;

                stringstream ss;
                ss << fixed << setprecision(2) << "CPU Usada: " << cpu_percentage << " %\n";
                result = ss.str();
            }
        }
    }
    pclose(file);
    return result;
}

string getMemoryUsage() {
    //Comando "free | grep Mem" para pegar uso de memória
    //Vai retornar: Mem: 16281140     5725836     1332820      ...
    //Sendo:        Mem:  TOTAL        USADO       LIVRE       ...

    const char* command = "free | grep Mem";
    char buffer[256];
    string result = "Memória usada: 0 kB\n";

    FILE* file = popen(command, "r");
    if(file) {
        //Armazena o retorno do comando na variável buffer
        if (fgets(buffer, sizeof(buffer), file) != nullptr) {
            
            //Gera um stringstream para extrair os valores gerados pelo comando (EM ORDEM)
            stringstream ss(buffer);
            string label;
            long total_kb, used_kb;

            ss >> label;                //O rótulo "Mem:" é armazenado e descartado em 'label'

            ss >> total_kb >> used_kb;  //Memória total e memória usada são armazenadas nas variáveis total_kb e used_kb

            stringstream result_ss;
            result_ss << "Memória usada: " << (used_kb/1024) << " MB\n";
            result = result_ss.str();
        }
        pclose(file);
    }
    return result;
}


void output(int clientSocket, pid_t pid, int tempo, int tipo, monitorControl& control) {
    while (control.keepRunning) {
        string result = (tipo == 1) ? getMemoryUsage() : getCPUUsage();
        send(clientSocket, result.c_str(), result.size(), 0);

        std::unique_lock<std::mutex> lock(control.mtx);

        if (control.cv.wait_for(lock, std::chrono::seconds(tempo), [&](){ 
            return !control.keepRunning;
        })) 
        {
            break;
        }
    }
}

void input(int clientSocket, pid_t pid, atomic<int>& client_count) {
    //Aumenta contador de clientes conectados
    client_count++;
    cout << "Quantidade de clientes conectados: " << client_count << endl;

    monitorControl memControl;
    monitorControl cpuControl;

    thread threadMem;
    thread threadCpu;

    char buffer[1024];
    int bytesReceived;


    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';

        cout << "Comando do cliente " << clientSocket << ": " << buffer << "\n";

        string cmd, param;
        int tempo = 1;
        istringstream iss(buffer);
        iss >> cmd >> param;

        for (auto & c: cmd) c = toupper(c);

        if (cmd == "MEMORIA" || cmd == "MEM") {
            try {
                tempo = (param.empty()) ? 1 : stoi(param);
                if (tempo <= 0) {
                    string msg = "Intervalo deve ser maior que zero.\n";
                    send(clientSocket, msg.c_str(), msg.size(), 0);
                    continue;
                }
            } catch (const exception& e) {
                string msg = "Intervalo inválido.\n";
                send(clientSocket, msg.c_str(), msg.size(), 0);
                continue;
            }

            if (memControl.keepRunning) {
                send(clientSocket, "ERRO: Monitor de memória já ativo.\n", 36, 0);

            } else {
                memControl.keepRunning = true;
                threadMem = thread(output, clientSocket, pid, tempo, 1, ref(memControl));
            }

        } else if (cmd == "CPU") {
            try {
                tempo = (param.empty()) ? 1 : stoi(param);
                if (tempo <= 0) {
                    string msg = "Intervalo deve ser maior que zero.\n";
                    send(clientSocket, msg.c_str(), msg.size(), 0);
                    continue;
                }
            } catch (const exception& e) {
                string msg = "Intervalo inválido.\n";
                send(clientSocket, msg.c_str(), msg.size(), 0);
                continue;
            }

            if (cpuControl.keepRunning) {
                send(clientSocket, "ERRO: Monitor de CPU ja ativo.\n", 31, 0);

            } else {
                cpuControl.keepRunning = true;
                if (threadCpu.joinable()) {
                    threadCpu.join();
                }
                threadCpu = thread(output, clientSocket, pid, tempo, 0, ref(cpuControl));
            }

        } else if (cmd == "QUIT") {
            for (auto & c: param) c = toupper(c);
            if (param == "MEM") {
                if(memControl.keepRunning) {
                    memControl.keepRunning = false;
                    memControl.cv.notify_one();
                    if (threadMem.joinable()) {
                        threadMem.join();
                    }
                    send(clientSocket, "Monitor de memoria interrompido.\n", 33, 0);

                    if(!cpuControl.keepRunning) {
                        sendMenu(clientSocket);
                    }
                }

            } else if (param == "CPU") {
                if(cpuControl.keepRunning) {
                    cpuControl.keepRunning = false;
                    cpuControl.cv.notify_one();
                    if (threadCpu.joinable()) {
                        threadCpu.join();
                    }
                    send(clientSocket, "Monitor de CPU interrompido.\n", 29, 0);

                    if(!memControl.keepRunning) {
                        sendMenu(clientSocket);
                    }
                }
            
            } else if (param == "ALL") {
                memControl.keepRunning = false;
                cpuControl.keepRunning = false;

                memControl.cv.notify_one();
                cpuControl.cv.notify_one();

                if (threadMem.joinable()) {
                    threadMem.join();
                }
                if (threadCpu.joinable()) {
                    threadCpu.join();
                }

                std::string msg = "Todos os monitores foram interrompidos.\n";
                send(clientSocket, msg.c_str(), msg.size(), 0);

                sendMenu(clientSocket);

            } else {
                send(clientSocket, "Use: Quit MEM ou Quit CPU\n", 27, 0);
            }

        } else if (cmd == "EXIT") {
            cout << "Cliente " << clientSocket << " pediu para desconectar.\n";
            send(clientSocket, "Desconectando...\n", 18, 0);
            break;
        } else {
            send(clientSocket, "Comando desconhecido.\n", 22, 0);
        }
        memset(buffer, 0, sizeof(buffer));
    }

    cout << "Cliente " << clientSocket << " desconectou.\n";

    memControl.keepRunning = false;
    cpuControl.keepRunning = false;
    memControl.cv.notify_one();
    cpuControl.cv.notify_one();

    if (threadMem.joinable()) threadMem.join();
    if (threadCpu.joinable()) threadCpu.join();

    close(clientSocket);

    //Diminui contador de clientes conectados
    client_count--;
    cout << "Quantidade de clientes conectados: " << client_count << endl;
}

int main(int argc, char *argv[]) {
    //Se não receber o limite de clientes, da erro e encerra o programa
    if(argc < 2) {
        cerr << "É necessário informar o limite de clientes.\n";
        return 1;
    }
    int client_limit = 0;

    //Bloco try catch para verificar se o argumento é um valor válido 
    try {
        client_limit = stoi(argv[1]);
        if(client_limit <= 0) {
            cerr << "Limite de clientes deve ser um número maior que zero.\n";
            return 1;
        }
    } catch (const exception& e) {
        cerr << "Limite de clientes inválido.\n";
        return 1;
    }
    cout << "Limite de clientes: " << client_limit << " clientes.\n";

    //Contador de clientes conectados
    atomic<int> client_count{0};

    //Cria lista de threads e socket dos clientes
    vector<pair<thread, int>> clientThreads;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    listen(serverSocket, 5);

    pid_t pid = getpid();

    cout << "Servidor aguardando conexão...\n";

    std::atomic<bool> running{true};

    // Thread para ler input do terminal do servidor
    std::thread serverInput([&running]() {
        std::string cmd;
        while (running) {
            std::getline(std::cin, cmd);
            if (cmd == "exit") {
                running = false;
                std::cout << "Encerrando servidor...\n";
                break;
            }
        }
    });

    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1; // 1 segundo
        timeout.tv_usec = 0; // 0 microsegundos

        int activity = select(serverSocket + 1, &readfds, nullptr, nullptr, &timeout);

        if (!running) break;

        if (activity > 0 && FD_ISSET(serverSocket, &readfds)) {
            int clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket < 0) {
                cerr << "Erro ao aceitar conexão.\n";
                continue;
            }

            //Verificador de limite de clientes
            if(client_count >= client_limit) {
                cout << "Limite de cliente atingido (" << client_count << "/" << client_limit << ").\n";
                const char* msg = "Servidor cheio. Tente novamente mais tarde.\n";
                send(clientSocket, msg, strlen(msg), 0);
                close(clientSocket);
                continue;
            }

            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);

            stringstream ss;
            ss << "<" << std::put_time(std::localtime(&in_time_t), "%H:%M:%S") << ">: CONECTADO!!\n";
            string connectedMsg = ss.str();
            send(clientSocket, connectedMsg.c_str(), connectedMsg.size(), 0);

            sendMenu(clientSocket);

            cout << "Cliente " << clientSocket << " conectado!! " << endl;

            //Guarda thread e socket do cliente na lista
            clientThreads.emplace_back(thread(input, clientSocket, pid, ref(client_count)), clientSocket);
        }
        // Se não teve atividade, apenas continua o loop para checar running
    }

    cout << "Encerrando conexão com " << client_count << " clientes...\n";

    //Fecha todos os sockets
    for(auto const& [thr, sock] : clientThreads) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    //Da join em todas as threads
    for(auto& [thr, sock] : clientThreads) {
        if(thr.joinable()) {
            thr.join();
        }
    }

    close(serverSocket);
    if (serverInput.joinable()) serverInput.join();

    return 0;
}
