/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"

//#include "user_iot_version.h"
#include "espconn.h"
//#include "user_json.h"
#include "user_webserver.h"
#include "start.h"
#include "answer.h"

//#include "upgrade.h"

#ifdef SERVER_SSL_ENABLE
#include "ssl/cert.h"
#include "ssl/private_key.h"
#endif

LOCAL struct station_config *sta_conf;
LOCAL struct softap_config *ap_conf;

//LOCAL struct secrty_server_info *sec_server;
//LOCAL struct upgrade_server_info *server;
//struct lewei_login_info *login_info;
LOCAL scaninfo *pscaninfo;
struct bss_info *bss;
struct bss_info *bss_temp;
struct bss_info *bss_head;

extern u16 scannum;

LOCAL uint32 PostCmdNeeRsp = 1;

uint8 upgrade_lock = 0;
LOCAL os_timer_t app_upgrade_10s;
LOCAL os_timer_t upgrade_check_timer;

/******************************************************************************
 * FunctionName : parse_url
 * Description  : parse the received data from the server
 * Parameters   : precv -- the received data
 *                purl_frame -- the result of parsing the url
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR parse_url(char *precv, URL_Frame *purl_frame){
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbuffer2 = NULL;
    char *pbufer = NULL;

    if (purl_frame == NULL || precv == NULL) {
        return;
    }

    pbuffer = (char *)os_strstr(precv, "Host:");
    pbuffer2 = (char *)os_strstr(precv, "\r\n\r\n");

    if (pbuffer != NULL) {
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        os_memcpy(pbuffer, precv, length);
        /* os_memset(purl_frame->pSelect, 0, URLSize); */
        /* os_memset(purl_frame->pCommand, 0, URLSize); */
        os_memset(purl_frame->pFilename, 0, URLSize);
	os_memset(purl_frame->pUID, 0, URLSize);
	os_memset(purl_frame->pSSID, 0, URLSize);
	os_memset(purl_frame->pPASSWORD, 0, URLSize);

        if (os_strncmp(pbuffer, "GET ", 4) == 0) {
            purl_frame->Type = GET;
            pbuffer += 4;
	   
	    pbuffer ++;
	    //str = (char *)os_strstr(pbuffer, "?"); 
	    str = (char *)os_strstr(pbuffer, " HTTP");
	    if (str != NULL) {
	        length = str - pbuffer;
	        os_memcpy(purl_frame->pFilename, pbuffer, length);
	    }

        } else if (os_strncmp(pbuffer, "POST ", 5) == 0) {
            purl_frame->Type = POST;

	    str = (char *)os_strstr(pbuffer2, "=");
	    if (str != NULL){
		str++;
		pbuffer2 = (char *)os_strstr(str, "&");
		length = pbuffer2 - str;
		os_memcpy(purl_frame->pUID, str, length);
		/* os_printf("POST step1: %s\n", purl_frame->pUID); */
		
		str = (char *)os_strstr(pbuffer2, "=");
		if (str != NULL){
			str++;
			pbuffer2 = (char *)os_strstr(str, "&");
			length = pbuffer2 - str;
			os_memcpy(purl_frame->pSSID, str, length);
			/* os_printf("POST step2: %s\n", purl_frame->pSSID); */

			str = (char *)os_strstr(pbuffer2, "=");
			if (str != NULL) {
			    str++;
			    //pbuffer2 = (char *)os_strstr(str, "&");
			    //length = pbuffer2 - str;
			    os_memcpy(purl_frame->pPASSWORD, str, os_strlen(str) + 1);
			    /* os_printf("POST step2: %s\n", purl_frame->pPASSWORD); */
			}
		}
	    }
        }
        os_free(pbufer);
    } else {
        return;
    }
}

LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;

LOCAL bool ICACHE_FLASH_ATTR save_data(char *precv, uint16 length)
{
    bool flag = false;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    uint16 headlength = 0;
    static uint32 totallength = 0;

    ptemp = (char *)os_strstr(precv, "\r\n\r\n");

    if (ptemp != NULL) {
        length -= ptemp - precv;
        length -= 4;
        totallength += length;
        headlength = ptemp - precv + 4;
        pdata = (char *)os_strstr(precv, "Content-Length: ");

        if (pdata != NULL) {
            pdata += 16;
            precvbuffer = (char *)os_strstr(pdata, "\r\n");

            if (precvbuffer != NULL) {
                os_memcpy(length_buf, pdata, precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
            }
        } else {
        	if (totallength != 0x00){
        		totallength = 0;
        		dat_sumlength = 0;
        		return false;
        	}
        }
        if ((dat_sumlength + headlength) >= 1024) {
        	precvbuffer = (char *)os_zalloc(headlength + 1);
            os_memcpy(precvbuffer, precv, headlength + 1);
        } else {
        	precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
        	os_memcpy(precvbuffer, precv, os_strlen(precv));
        }
    } else {
        if (precvbuffer != NULL) {
            totallength += length;
            os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
        } else {
            totallength = 0;
            dat_sumlength = 0;
            return false;
        }
    }

    if (totallength == dat_sumlength) {
        totallength = 0;
        dat_sumlength = 0;
        return true;
    } else {
        return false;
    }
}

LOCAL bool ICACHE_FLASH_ATTR check_data(char *precv, uint16 length)
{
        //bool flag = true;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    char *tmp_precvbuffer;
    uint16 tmp_length = length;
    uint32 tmp_totallength = 0;
    
    ptemp = (char *)os_strstr(precv, "\r\n\r\n");
    
    if (ptemp != NULL) {
        tmp_length -= ptemp - precv;
        tmp_length -= 4;
        tmp_totallength += tmp_length;
        
        pdata = (char *)os_strstr(precv, "Content-Length: ");
        
        if (pdata != NULL){
            pdata += 16;
            tmp_precvbuffer = (char *)os_strstr(pdata, "\r\n");
            
            if (tmp_precvbuffer != NULL){
                os_memcpy(length_buf, pdata, tmp_precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
                os_printf("A_dat:%u,tot:%u,lenght:%u\n",dat_sumlength,tmp_totallength,tmp_length);
                if(dat_sumlength != tmp_totallength){
                    return false;
                }
            }
        }
    }
    return true;
}

/******************************************************************************
 * FunctionName : data_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                responseOK -- true or false
 *                psend -- The send data
 * Returns      :
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend){
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK) {
        os_sprintf(httphead,
                   "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nServer: esp8266AP\r\n",
                   psend ? os_strlen(psend) : 0);

        if (psend) {
            os_sprintf(httphead + os_strlen(httphead),
                       "Content-type: text/html\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        } else {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    } else {
        os_sprintf(httphead, "HTTP/1.1 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend) {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_send(ptrespconn, pbuf, length);
#else
        espconn_send(ptrespconn, pbuf, length);
#endif
    } else {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_send(ptrespconn, httphead, length);
#else
        espconn_send(ptrespconn, httphead, length);
#endif
    }

    if (pbuf) {
        os_free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : response_send
 * Description  : processing the send result
 * Parameters   : arg -- argument to set for client or server
 *                responseOK --  true or false
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR response_send(void *arg, bool responseOK){
    struct espconn *ptrespconn = arg;

    data_send(ptrespconn, responseOK, NULL);
}

/******************************************************************************
 * FunctionName : webserver_recv
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length)
{
    URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d receive\n", ptrespconn->proto.tcp->remote_ip[0],
        		ptrespconn->proto.tcp->remote_ip[1],ptrespconn->proto.tcp->remote_ip[2],
        		ptrespconn->proto.tcp->remote_ip[3],ptrespconn->proto.tcp->remote_port);

    os_printf("len:%u\n",length);
        if(check_data(pusrdata, length) == false){
            return;
        }
        parse_flag = save_data(pusrdata, length);
        if (parse_flag == false) {
        	response_send(ptrespconn, false);
        }

        os_printf("%s\n", precvbuffer);
        pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
        parse_url(precvbuffer, pURL_Frame);

        switch (pURL_Frame->Type){
        case GET:
            os_printf("We have a GET request.\n");
            if (os_strcmp(pURL_Frame->pFilename, "start.html") == 0){
                data_send(ptrespconn, true, start_page);
            }else{
                os_printf("file requested: %s\n", pURL_Frame->pFilename);
                response_send(ptrespconn, false); //not ok
            }
            break;
        case POST:
            os_printf("We have a POST request.\n");
            //pParseBuffer = (char *)os_strstr(precvbuffer, "\r\n\r\n");
	    os_printf("UID: %s\n", pURL_Frame->pUID);
	    os_printf("SSID: %s\n", pURL_Frame->pSSID);
	    os_printf("UID: %s\n", pURL_Frame->pPASSWORD);
            data_send(ptrespconn, true, answer_page);
            break;
        default:
            break;
        }

}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_discon(void *arg)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
}

LOCAL ICACHE_FLASH_ATTR void webserver_con(void *arg){
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d connect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
}

/******************************************************************************
 * FunctionName : user_accept_listen
 * Description  : server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, webserver_recv);
    espconn_regist_reconcb(pesp_conn, webserver_recon);
    espconn_regist_disconcb(pesp_conn, webserver_discon);
    espconn_regist_connectcb(pesp_conn, webserver_con);
}

/******************************************************************************
 * FunctionName : user_webserver_init
 * Description  : parameter initialize as a server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_webserver_init(uint32 port)
{
    LOCAL struct espconn esp_conn;
    LOCAL esp_tcp esptcp;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&esp_conn, webserver_listen);

#ifdef SERVER_SSL_ENABLE
    espconn_secure_set_default_certificate(default_certificate, default_certificate_len);
    espconn_secure_set_default_private_key(default_private_key, default_private_key_len);
    espconn_secure_accept(&esp_conn);
#else
    espconn_accept(&esp_conn);
#endif
}
