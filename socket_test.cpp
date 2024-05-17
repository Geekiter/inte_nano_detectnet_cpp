#include <iostream>
// #include "Socket.h"
#include <string.h>
#include <jetson-utils/Socket.h>
#include "cJSON.h"
#include "jetson-utils/Socket.h"
using namespace std;

// 启动socket，监听127.0.0.1:8899，有请求时返回"hello pong"
int main() {
    // create a new socket
    Socket *socket = Socket::Create(SOCKET_TCP);
    if (!socket) {
        printf("failed to create socket\n");
        return 0;
    }

    // bind the socket to a local port
    if (!socket->Bind("192.168.31.189", 8899)) {
        printf("failed to bind socket\n");
        return 0;
    }

    if (!socket->Accept(0)) {
        printf("failed to accept socket\n");
        return 0;
    }
    printf("server is running\n");

    while (true) {
        // receive a message
        uint8_t buffer[1024];
        const char *receiveData = NULL;
        size_t bytes = socket->Recieve(buffer, sizeof(buffer));
        if (bytes == 0) {
            printf("failed to receive message\n");
            return 0;
        } else {
            printf("received message: %s\n", buffer);
            cJSON *cjson_receive = cJSON_Parse((char *) buffer);
            receiveData = cJSON_Print(cjson_receive);
            printf("%s\n", receiveData);

            // cJSON *fStickLat = cJSON_GetObjectItem(cjson_receive, "fStickLat");
            // double m_fStickLat = cJSON_GetNumberValue(fStickLat);
            // printf("%.10f\n", m_fStickLat);
            // printf("m_fStickLat: %.10f\n", m_fStickLat);
            cJSON *fpath = cJSON_GetObjectItem(cjson_receive, "fpath");
            char *m_fpath = cJSON_GetStringValue(fpath);
            printf("m_fpath: %s\n", m_fpath);
            cJSON_Delete(cjson_receive);

            // 检测到socket等于shutdown，关闭socket
            if (strcmp((char *) buffer, "shutdown") == 0) {
                printf("shutting down\n");
                break;
            }
        }
    }
}
