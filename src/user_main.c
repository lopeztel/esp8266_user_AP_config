#include "user_config.h"
#include "user_interface.h"
#include "osapi.h"
#include "driver/uart.h"
#include "c_types.h"
#include "user_webserver.h"

void ICACHE_FLASH_ATTR wifi_event_handler_cb(System_Event_t *event){
    switch (event->event)
    {
    case EVENT_SOFTAPMODE_STACONNECTED:
        os_printf("\nconnection to softAP!\n");
        /* struct station_info* station;
        station = wifi_softap_get_station_info();
        os_printf("bssid : " MACSTR ", ip : " IPSTR "/n",MAC2STR(station->bssid), IP2STR(&station->ip)); */
        break;
    
    case EVENT_SOFTAPMODE_STADISCONNECTED:
        os_printf("\ndisconnection from softAP!\n");
        break;
    
    default:
        break;
    }
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void){
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 8;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void){
}

bool ICACHE_FLASH_ATTR wifi_set_mode(uint8 mode){
    if(!mode){
        bool s = wifi_set_opmode(mode);
        wifi_fpm_open();
        wifi_fpm_set_sleep_type(MODEM_SLEEP_T);
        wifi_fpm_do_sleep(0xFFFFFFFF);
        return s;
    }
    wifi_fpm_close();
    return wifi_set_opmode(mode);
}

bool ICACHE_FLASH_ATTR start_wifi_ap(const char * ssid, const char * pass){
    uint8 mode = wifi_get_opmode();
    if((mode & SOFTAP_MODE) == 0){
        mode |= SOFTAP_MODE;
        if(!wifi_set_mode(mode)){
            os_printf("Failed to enable AP mode!\n");
            return false;
        }
    }
    if(!ssid){
        os_printf("No SSID Given. Will start the AP saved in flash\n");
        return true;
    }
    struct softap_config config;
    os_bzero(&config, sizeof(struct softap_config));
    os_sprintf(config.ssid, ssid);
    if(pass){
        os_sprintf(config.password, pass);
    }
    config.authmode = AUTH_WPA_WPA2_PSK;
    config.max_connection = 5;
    if (wifi_softap_set_config(&config)){
        os_printf("\nset softAP config\n");
        return true;
    } else {
        return false;
    }
    
}

bool ICACHE_FLASH_ATTR start_wifi_dhcps(){
    struct ip_info info;
    if (!wifi_softap_dhcps_stop()){
        os_printf("\nsoftAP couldn't stop dhcps\n");
        return false;
    }
    //ip address of AP
    IP4_ADDR(&info.ip, 192,168,5,1);
    IP4_ADDR(&info.gw, 192,168,5,1);
    IP4_ADDR(&info.netmask, 255,255,255,0);
    if (!wifi_set_ip_info(SOFTAP_IF, &info)){
        os_printf("\nsoftAP couldn't set ip info\n");
        return false;
    }

    //dhcps lease
    struct dhcps_lease dhcp_lease;
    IP4_ADDR(&dhcp_lease.start_ip, 192, 168, 5, 100);
    IP4_ADDR(&dhcp_lease.end_ip, 192, 168, 5, 105);
    if (!wifi_softap_set_dhcps_lease(&dhcp_lease)){
        os_printf("\nsoftAP couldn't set dhcps lease\n");
        return false;
    }

    if (!wifi_softap_dhcps_start()){
        os_printf("\nsoftAP couldn't start dhcps\n");
        return false;
    }
    return true;
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_init(void)
{
    // Configure the UART
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
    // Enable system messages
    system_set_os_print(1);
    //create an event handler
    wifi_set_event_handler_cb(wifi_event_handler_cb);
    //start as Access Point
    if(start_wifi_ap(AP_SSID, AP_PASSWORD)){
        if (!start_wifi_dhcps()){
            os_printf("\nsoftAP dhcps config error\n");
        }
        user_webserver_init(SERVER_PORT);
    } else {
        os_printf("\nsoftAP config error\n");
    }
    //Print SDK version and which userbin is used (0=user1.bin, 1=user2.bin)
    os_printf("\nSDK version:%s, rom %d\n", system_get_sdk_version(), system_upgrade_userbin_check());
}