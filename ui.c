#include "ui.h"
#include "display.h"
void ui_show_ap_mode(const char *ip) {
    display_clear();
    display_draw_string(0, 0, "Welcome to SYNAPSE");
    display_draw_string(0, 16, "Connect PC to: ");
    display_draw_string(0, 32, ip);
    display_update();
}
void ui_show_pc_connected(const char *ip) {
    display_clear();
    display_draw_string(0, 0,  "PC Connected");
    display_draw_string(0, 16, "AP IP:");
    display_draw_string(0, 24, ip);
    display_draw_string(0, 40, "Waiting for");
    display_draw_string(0, 48, "WiFi Credentials");
    display_update();
}
void ui_show_received_credentials(const char *ssid, const char *password) {
    	display_clear();
    	display_draw_string(0,0,"WiFi Credentials");
    	display_draw_string(0,18,"SSID:");
    	display_draw_string(36,18,ssid);
    	display_draw_string(0,36,"PASS:");
    	display_draw_string(36,36,password);
    	display_update();
}
void ui_show_wifi_connected(const char *ip) {
    	display_clear();
    	display_draw_string(0, 0, "WiFi Connected");
    	display_draw_string(0, 20, "IP:");
    	display_draw_string(24, 20, ip);
    	display_update();
}
void ui_show_wifi_failed(void) {
    	display_clear();
    	display_draw_string(0, 0, "WiFi Failed");
    	display_draw_string(0, 20, "Check SSID");
    	display_draw_string(0, 36, "or Password");
    	display_update();
}
void ui_show_connecting(const char *ssid) {
    display_clear();
    display_draw_string(0, 0, "Connecting...");
    display_draw_string(0, 20, "SSID:");
    display_draw_string(36, 20, ssid);
    display_draw_string(0, 42, "Please Wait-");
    display_update();
}
void ui_show_tcp_listening(const char *ip) {
  display_clear();
  display_draw_string(0,0,"TCP Server");
 display_draw_string(0,20,"Listening");
 display_draw_string(0,32,"IP:");
    display_draw_string(24,32,ip);
 
 display_update();
}
void ui_show_tcp_connected(void){
    display_clear();
    display_draw_string(0,0,"SYNAPSE");
    display_draw_string(0,20,"Setup Successful");
    display_update();
}
void ui_show_acquisition_running(void) {
    display_clear();
    display_draw_string(0,0,"Oscilloscope");
    display_draw_string(0,20,"Streaming");
    display_draw_string(0,40,"Running");
    display_update();
}
