    // C++ program to illustrate the client application in the
    // socket programming
    #include <cstring>
    #include <iostream>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <thread>
    #include <atomic>
    #include <sys/select.h>

    using namespace std;

    // Flag para controlar conexão com o servidor, atualiza quando o servidor é fechado
    // Quebra o loop da main quando connected = false
    atomic<bool> connected{true};

    void input(int clientSocket) {
        char buffer[1024];
        int bytesReceived;

        // Recebe mensagem do servidor
        while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0'; // Garante terminação
            cout << "\r\033[K" << buffer << "> " << flush;
        }
        cout << "\r\033[K"; // Limpa linha
        cout << "Conexão com o servidor perdida!!" << endl;
        connected = false; 
    }

    int main()
    {
        // Criando o socket
        int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

        // Especificando o endereço
        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(8080);
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        // Enviando solicitação de conexão
        while(true){
            int success = connect(clientSocket, (struct sockaddr*)&serverAddress,sizeof(serverAddress));
            if(success == 0) {
                cout << "Conectado ao servidor.\n";
                break;
            } else {
                cerr << "Falha na conexão. Tentando novamente...\n";
                this_thread::sleep_for(chrono::seconds(5));
            }
        }

        thread threadInput(input, clientSocket);

        cout << "> " << flush;

        while(connected) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);

            timeval timeout;
            timeout.tv_sec = 1; // Seta timeout de 1 segundo
            timeout.tv_usec = 0;

            select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);

            // Se tiver input, roda "getline" por 1 segundo para não ficar preso
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                string message;
                if (getline(cin, message)) {
                    if (!message.empty()) {
                        send(clientSocket, message.c_str(), message.size(), 0);
                    }

                    string temp_msg = message;
                    for(auto &c : temp_msg) c = toupper(c);
                    if (temp_msg == "EXIT") {
                        break;
                    }
                    cout << "> " << flush;
                } else {
                    connected = false;
                }
            }
        }

        cout << "Encerrando a conexão!!" << endl;

        shutdown(clientSocket, SHUT_RDWR);

        // Fechando socket
        if (threadInput.joinable()) threadInput.join();
        close(clientSocket);

        return 0;
    }