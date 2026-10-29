#include "ui.h"
#include "tcp_server.h"
#include "acquisition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdbool.h>
static const char *TAG="TCP_SERVER";
static char server_ip[16];
#define TCP_CAPTURE_SAMPLES 2048  // samples sent per GET_DATA request
static void tcp_server_task(void *pvParameters){
    int listen_sock=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
    if(listen_sock<0){
        ESP_LOGE(TAG,"Socket failed");
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in server_addr={0};
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(5000);
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(listen_sock,(struct sockaddr *)&server_addr,sizeof(server_addr))<0){
        ESP_LOGE(TAG,"Bind failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    if(listen(listen_sock,1)<0){
        ESP_LOGE(TAG,"Listen failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG,"Listening on port 5000");
    ui_show_tcp_listening(server_ip);
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len=sizeof(client_addr);
        int client_sock=accept(listen_sock,(struct sockaddr *)&client_addr,&client_len);
        if(client_sock<0) continue;
        ESP_LOGI(TAG,"TCP Client Connected");
        ui_show_tcp_connected();
        char cmd[64];
        bool client_alive = true;
        while(client_alive){
            int len=recv(client_sock,cmd,sizeof(cmd)-1,0);
            if(len<=0) break;
            cmd[len]='\0';
            if(strcmp(cmd,"PING")==0){
                send(client_sock,"PONG",4,0);
            }
            else if(strcmp(cmd,"GET_DATA")==0){
                static uint16_t capture_buf[TCP_CAPTURE_SAMPLES];
                size_t n = acquisition_read_samples(capture_buf, TCP_CAPTURE_SAMPLES);
                uint32_t count = (uint32_t)n;
                float rate_hz = acquisition_get_sample_rate_hz();
                // SYNAPSE_GUI's receive_frame() expects a 4-byte little-endian
                // float32 measured sample rate, then a 4-byte little-endian
                // sample count, then count*2 bytes of uint16 samples.
                if (send(client_sock, &rate_hz, sizeof(rate_hz), 0) <= 0) break;
                if (send(client_sock, &count, sizeof(count), 0) <= 0) break;
                if (n > 0 && send(client_sock, capture_buf, n * sizeof(uint16_t), 0) <= 0) break;
            }
            else if(strcmp(cmd,"STREAM")==0){
                // GUI sends "STREAM" once, then just keeps reading frames until
                // it disconnects (see dso_page.py StreamWorker) - it never sends
                // another command, so take over the connection here.
                ESP_LOGI(TAG,"Client started STREAM");
                static uint16_t stream_buf[TCP_CAPTURE_SAMPLES];
                while(1){
                    size_t n = acquisition_read_samples(stream_buf, TCP_CAPTURE_SAMPLES);
                    if (n == 0){
                        vTaskDelay(pdMS_TO_TICKS(1));
                        continue;
                    }
                    uint32_t count = (uint32_t)n;
                    float rate_hz = acquisition_get_sample_rate_hz();
                    if (send(client_sock, &rate_hz, sizeof(rate_hz), 0) <= 0){
                        client_alive = false;
                        break;
                    }
                    if (send(client_sock, &count, sizeof(count), 0) <= 0){
                        client_alive = false;
                        break;
                    }
                    if (send(client_sock, stream_buf, n * sizeof(uint16_t), 0) <= 0){
                        client_alive = false;
                        break;
                    }
                }
            }
        }
        close(client_sock);
        ESP_LOGI(TAG,"TCP Client Disconnected");
    }
}
void tcp_server_start(const char *ip){
    strncpy(server_ip,ip,sizeof(server_ip)-1);
    server_ip[sizeof(server_ip)-1]='\0';
    acquisition_init();
    xTaskCreate(tcp_server_task,"tcp_server",4096,NULL,5,NULL);
}
