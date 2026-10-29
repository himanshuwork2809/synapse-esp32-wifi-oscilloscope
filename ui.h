#ifndef UI_H
#define UI_H
void ui_show_ap_mode(const char *ip);
void ui_show_pc_connected(const char *ip);
void ui_show_waiting_credentials(void);
void ui_show_connecting_wifi(const char *ssid);
void ui_show_wifi_connected(const char *ip);
void ui_show_ready(void);
void ui_show_received_credentials(const char *ssid, const char *password);
void ui_show_received_credentials(const char *ssid, const char *password);
void ui_show_wifi_connected(const char *ip);
void ui_show_wifi_failed(void);
void ui_show_connecting(const char *ssid);
void ui_show_tcp_listening(const char *ip);
void ui_show_tcp_connected(void);
#endif
