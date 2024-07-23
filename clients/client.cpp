#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

const int PORT = 1988;
const int BUFFER_SIZE = 1024;

void downloadFile(int socket, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл для записи: " << filename << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived;

    while ((bytesReceived = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        file.write(buffer, bytesReceived);
    }

    if (bytesReceived == 0) { // EOF
        std::cout << "Файл успешно скачан." << std::endl;
    } else {
        std::cerr << "Ошибка при скачивании файла." << std::endl;
    }

    file.close();
}

void uploadFile(int socket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Файл '" << filename << "' не найден." << std::endl;
        return;
    }
    char buffer[BUFFER_SIZE];

    while (file.read(buffer, sizeof(buffer))) {
        ssize_t bytesSent = send(socket, buffer, file.gcount(), 0);
        if (bytesSent < 0) {
            std::cerr << "Ошибка при отправке файла: " << errno << std::endl;
            return;
        }
    }
    if (file.gcount() > 0) {
        send(socket, buffer, file.gcount(), 0);
    }
    file.close();
}

void listFiles(int socket) {
    char command[] = "LIST";
    send(socket, command, strlen(command), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived;
    bool endReceived = false;

    std::cout << "Список файлов на сервере:\n";
    while ((bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::string data(buffer, bytesReceived);
        std::cout << data;
        if (data.find("END") != std::string::npos) {
            endReceived = true;
            break;
        }
    }

    if (!endReceived) {
        std::cerr << "Не удалось получить список файлов полностью." << std::endl;
    }
    std::cout << std::endl;
}

void shutdownServer(int socket) {
    char command[] = "SHUTDOWN";
    send(socket, command, strlen(command), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived > 0) {
        std::cout.write(buffer, bytesReceived);
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }
#endif

    int clientSocket;
    struct sockaddr_in serverAddr;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("192.168.31.128");

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Не удалось подключиться к серверу. Убедитесь, что сервер запущен и доступен." << std::endl;
        return 1;
    }

    while (true) {
        std::cout << "Введите команду (LIST, GET <filename>, UPLD <filename>, SHUTDOWN, EXIT): ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "LIST") {
            listFiles(clientSocket);
        } else if (command.rfind("GET ", 0) == 0) {
            std::string filename = command.substr(4);
            char fullCommand[BUFFER_SIZE];
            snprintf(fullCommand, BUFFER_SIZE, "GET %s", filename.c_str());
            send(clientSocket, fullCommand, strlen(fullCommand), 0);
            downloadFile(clientSocket, filename);
            close(clientSocket);
            clientSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
                std::cerr << "Не удалось подключиться к серверу. Убедитесь, что сервер запущен и доступен." << std::endl;
                return 1;
            }
        } else if (command.rfind("UPLD ", 0) == 0) {
            std::string filename = command.substr(5);
            char fullCommand[BUFFER_SIZE];
            snprintf(fullCommand, BUFFER_SIZE, "UPLD %s", filename.c_str());
            send(clientSocket, fullCommand, strlen(fullCommand), 0);
            uploadFile(clientSocket, filename);
            close(clientSocket);
            clientSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
                std::cerr << "Не удалось подключиться к серверу. Убедитесь, что сервер запущен и доступен." << std::endl;
                return 1;
            }
        } else if (command == "EXIT") {
            char exitCommand[] = "EXIT";
            send(clientSocket, exitCommand, strlen(exitCommand), 0);
            break;
        } else if (command == "SHUTDOWN") {
            shutdownServer(clientSocket);
            break;
        } else {
            std::cout << "Неизвестная команда." << std::endl;
        }
    }

    close(clientSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
