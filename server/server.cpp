#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sstream>
#include <cstring>
#include <netdb.h>
#include <signal.h>

const int PORT = 1988;
const int BUFFER_SIZE = 1024;
bool serverRunning = true;

void sendFile(int clientSocket, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл: " << filename << std::endl;
        std::string errorMsg = "ERROR";
        send(clientSocket, errorMsg.c_str(), errorMsg.size(), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer))) {
        ssize_t bytesSent = send(clientSocket, buffer, file.gcount(), 0);
        if (bytesSent < 0) {
            std::cerr << "Ошибка при отправке файла" << std::endl;
            file.close();
            return;
        }
    }
    if (file.gcount() > 0) {
        send(clientSocket, buffer, file.gcount(), 0);
    }

    file.close();
}
void shutdownServer() {
    serverRunning = false;
    std::cout << "Сервер выключается..." << std::endl;
}
void handleUploadCommand(int clientSocket, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл для записи: " << filename << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        file.write(buffer, bytesRead);
    }

    file.close();
}

void sendFileList(int clientSocket) {
    DIR* dir;
    struct dirent* ent;
    std::ostringstream fileList;

    if ((dir = opendir(".")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            fileList << ent->d_name << "\n";
        }
        closedir(dir);
    } else {
        std::cerr << "Не удалось открыть директорию" << std::endl;
        return;
    }

    std::string fileListStr = fileList.str();
    send(clientSocket, fileListStr.c_str(), fileListStr.size(), 0);


    std::string endMessage = "END\n";
    send(clientSocket, endMessage.c_str(), endMessage.size(), 0);
}

void sendMessage(int clientSocket, const std::string& message) {
    send(clientSocket, message.c_str(), message.size(), 0);
}

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 10);

    std::cout << "Сервер запущен. Ожидание подключений...\n";

    int clientId = 0;
    while (true) {
        addr_size = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addr_size);

        clientId++;
        std::cout << "Клиент " << clientId << " подключился: " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;

        char command[BUFFER_SIZE] = {0};
        ssize_t bytesRead = recv(clientSocket, command, BUFFER_SIZE - 1, 0);
        if (bytesRead <= 0) {
            close(clientSocket);
            continue;
        }
        std::string cmdStr(command, bytesRead);

        std::cout << "Получена команда: " << cmdStr << std::endl;

        if (cmdStr == "EXIT") {
            std::string exitMessage = "Клиент вышел\n";
            send(clientSocket, exitMessage.c_str(), exitMessage.size(), 0);
            close(clientSocket);
        } else if (cmdStr == "LIST") {
            sendFileList(clientSocket);
        } else if (cmdStr.substr(0, 4) == "GET ") {
            std::string filename = cmdStr.substr(4);
            std::cout << "Запрашиваемый файл: " << filename << std::endl;
            sendFile(clientSocket, filename);
            close(clientSocket);
        } else if (cmdStr.substr(0, 5) == "UPLD ") {
            std::string filename = cmdStr.substr(5);
            std::cout << "Загружаемый файл: " << filename << std::endl;
            handleUploadCommand(clientSocket, filename);
            close(clientSocket);
        } else if (cmdStr == "SHUTDOWN") {
            std::string shutdownMessage = "Сервер выключается...";
            send(clientSocket, shutdownMessage.c_str(), shutdownMessage.size(), 0);
            close(clientSocket);
            shutdownServer();
            close(serverSocket);
    		std::cout << "Сервер завершил работу. Возврат в командную строку." << std::endl;
    		return 0;
        }else {
            std::cerr << "Неизвестная команда: " << cmdStr << std::endl;
            close(clientSocket);
        }
    }

    close(serverSocket);
    return 0;
}

