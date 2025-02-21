#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/socket.h>
#include <wpa_ctrl.h>
#include <errno.h>
#include <signal.h>

#include "wpa_command.h"

static struct wpa_ctrl *monitor_conn;
static int exit_sockets[2];
static char reply[4096] = {0};
static size_t reply_len = sizeof(reply);

struct wifi_command_pack  wifi_command_pack_sum[] = {
    {WIFI_COMMAND_SCAN, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_ADDNETWORK, WIFI_COMMAND_ACK_INT},
    {WIFI_COMMAND_SETNETWORK_SSID, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SETNETWORK_SCANSSID, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SETNETWORK_PSK, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SETNETWORK_KEYMETH, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SETNETWORK_AUTHALG, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SETNETWORK_WEPKEY0, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SETNETWORK_WEPTXKEYID, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_ENABLENETWORK, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SELECTNETWORK, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_RECONFIGURE, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_REATTACH, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_GET_STATUS, WIFI_COMMAND_ACK_INT},
    {WIFI_COMMAND_GET_SIGNAL, WIFI_COMMAND_ACK_INT},
    {WIFI_COMMAND_SAVECONFIG, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_SCAN_R, WIFI_COMMAND_ACK_STRING},
    {WIFI_COMMAND_LISTNETWORK, WIFI_COMMAND_ACK_INT},
    {WIFI_COMMAND_DISABLENETWORK, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_GET_CHANNEL, WIFI_COMMAND_ACK_INT},
    {WIFI_COMMAND_DISCONNECT, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_RECONNCET, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_REMOVENETWORK, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_GET_CURRENT_SSID, WIFI_COMMAND_ACK_STRING},
    {WIFI_COMMAND_BLACKLIST_CLEAR, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_GET_BSSID, WIFI_COMMAND_ACK_STRING},
    {WIFI_COMMAND_SET_COUNTRY, WIFI_COMMAND_ACK_OK},
    {WIFI_COMMAND_GET_COUNTRY, WIFI_COMMAND_ACK_STRING},
    {WIFI_COMMAND_GET_WIFI_INFO,WIFI_COMMAND_ACK_STRING},
};

static int get_line_from_buf(int index, char *line, char *buf)
{
    int i = 0;
    int j;
    int endcnt = -1;
    char *linestart = buf;
    int len;

    while (1) {
        if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == '\0') {
            endcnt++;
            if (index == endcnt) {
                len = &buf[i] - linestart;
                strncpy(line, linestart, len);
                line[len] = '\0';
                return 0;
            } else {
                for (j = i + 1; buf[j]; ) {
                    if (buf[j] == '\n' || buf[j] == '\r')
                        j++;
                    else
                        break;
                }
                if (!buf[j])
                    return -1;
                linestart = &buf[j];
                i = j;
            }
        }

        if (!buf[i])
            return -1;
        i++;
    }
}

static int wpa_reduce_repeat_ssid (struct wifi_scan_list *list, struct wifi_scan_res *res)
{
    int i = 0;

    for (i = 0; i < list->num; i++) {
        if ((strcmp (list->ssid[i].ssid, res->ssid) == 0) && (list->ssid[i].key_mgmt == res->key_mgmt)) {
            if (list->ssid[i].sig < res->sig) {
                memcpy(&list->ssid[i], res, sizeof(struct wifi_scan_res));
            }
            return 0;
        }
    }

    memcpy(&list->ssid[i], res, sizeof(struct wifi_scan_res));
    list->num += 1;

    return 0;
}

static int wpa_get_scan_r(char *reply, int reply_len, struct wifi_scan_list *list)
{
    int i = 0;
    char line[512] = {0};
    char key_type[64] = {0};
    struct wifi_scan_res ap;

    memset(&ap, 0, sizeof(ap));

    for (i = 1; !get_line_from_buf(i, line, reply); i++) {
        sscanf(line, "%s\t%d\t%d\t%s\t%[^\n]s", ap.mac_addr, &ap.freq, &ap.sig, key_type, ap.ssid);

        if (strstr(key_type, "WPA2") != NULL) {
            ap.key_mgmt = WIFI_KEY_WPA2PSK;
        } else if (strstr(key_type, "WPA") != NULL) {
            ap.key_mgmt = WIFI_KEY_WPAPSK;
        } else if (strstr(key_type, "WEP") != NULL) {
            ap.key_mgmt = WIFI_KEY_WEP;
        } else if (strncmp(key_type, "[ESS]", strlen("[ESS]")) == 0) {
            ap.key_mgmt = WIFI_KEY_NONE;
        } else {
            ap.key_mgmt = WIFI_KEY_OTHER;
        }

        wpa_reduce_repeat_ssid(list, &ap);
    }

    return 0;
}

static int wifi_reply_analyse(int cmd, char *reply, int reply_len, void *res, int res_len)
{
    int index = -1;
    int i = 0;
    char line[256] = {0};

    for (i = 0; i < sizeof(wifi_command_pack_sum) / sizeof(struct wifi_command_pack); i++) {
        if (cmd == wifi_command_pack_sum[i].cmd) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        return -1;
    }

    switch (wifi_command_pack_sum[index].ack) {
    case WIFI_COMMAND_ACK_OK: {
        if (strncmp(reply, "OK", res_len > strlen("OK") ? strlen("OK") : res_len) == 0) {
            *(int *)res = 0;
        } else {
            *(int *)res = -1;
        }
        res_len = sizeof(int);
        break;
        }
    case WIFI_COMMAND_ACK_INT: {
        if (cmd == WIFI_COMMAND_GET_SIGNAL) {
            for (i = 0; !get_line_from_buf(i, line, reply); i++) {

                if (strstr(line, "RSSI") != NULL) {

                    *(char *)res = atoi(&line[5]);
                    return 0;
                }
            }

            *(char *)res = 0;

        } else if (cmd == WIFI_COMMAND_GET_CHANNEL) {
            for (i = 0; !get_line_from_buf(i, line, reply); i++) {

                if (strstr(line, "FREQUENCY") != NULL) {

                    *(int *)res = atoi(&line[10]);
                    return 0;
                }
            }

            *(int *)res = 0;

        } else if(cmd == WIFI_COMMAND_GET_STATUS) {
            for (i = 0; !get_line_from_buf(i, line, reply); i++) {
                if (strstr(line, "wpa_state=COMPLETED")) {
                    *(int *)res = WIFI_CONNECTED;
                    return 0;
                } else if (strstr(line, "wpa_state=SCANNING")) {
                    *(int *)res = WIFI_SCANING;
                    return 0;
                }

                *(int *)res = WIFI_UNCONNECTED;
            }

        } else if (cmd == WIFI_COMMAND_LISTNETWORK) {
            *(int *)res = 0;
            for (i = 1; !get_line_from_buf(i, line, reply); i++) {
                if (strstr(line, "[DISABLED]") == NULL) {
                    *(int *)res = 1;
                    break;
                }
            }
        } else {
            if (sscanf(reply, "%d", (int *)res) == 0) {
                *(int *)res = -1;
            }
        }

        break;
    }
    case WIFI_COMMAND_ACK_STRING: {
        if (cmd == WIFI_COMMAND_SCAN_R) {
            if (wpa_get_scan_r(reply, reply_len, res) == 0) {

            }
        } else if (cmd == WIFI_COMMAND_GET_CURRENT_SSID) {
            for (i = 0; !get_line_from_buf(i, line, reply); i++) {
                if (strncmp(line, "ssid=", strlen("ssid=")) == 0) {
                    strncpy(res, line + 5, strlen(line) - 5);
                }
            }
        } else if(cmd == WIFI_COMMAND_GET_BSSID) {
            unsigned char bssid[6] = {0x0};
            unsigned int a1,a2,a3,a4,a5,a6;
            for (i = 0; !get_line_from_buf(i, line, reply); i++) {
                if (strncmp(line, "bssid=",strlen("bssid=")) == 0 ) {
                    sscanf(line + 6, "%02x:%02x:%02x:%02x:%02x:%02x",&a1,&a2,&a3,&a4,&a5,&a6);
                    bssid[0] = a1 & 0xFF;
                    bssid[1] = a2 & 0xFF;
                    bssid[2] = a3 & 0xFF;
                    bssid[3] = a4 & 0xFF;
                    bssid[4] = a5 & 0xFF;
                    bssid[5] = a6 & 0xFF;
                    memcpy(res,bssid,sizeof(bssid));
                    return 0;
                }
            }
        } else if(cmd == WIFI_COMMAND_GET_WIFI_INFO) {
            int a1,a2,a3,a4,a5,a6;
            WF_CONNECTED_AP_INFO_S ap_info_s;
            memset(&ap_info_s,0,sizeof(WF_CONNECTED_AP_INFO_S));
            a1 = a2 = a3 = a4 = a5 = a6 = 0;

            WF_CONNECTED_AP_INFO_S *ap_info_p = (WF_CONNECTED_AP_INFO_S *)res;

            for (i = 0; !get_line_from_buf(i, line, reply); i++) {
                if (strncmp(line, "bssid=",strlen("bssid=")) == 0 ) {
                    sscanf(line + 6, "%02x:%02x:%02x:%02x:%02x:%02x",&a1,&a2,&a3,&a4,&a5,&a6);
                    ap_info_s.bssid[0] = a1 & 0xFF;
                    ap_info_s.bssid[1] = a2 & 0xFF;
                    ap_info_s.bssid[2] = a3 & 0xFF;
                    ap_info_s.bssid[3] = a4 & 0xFF;
                    ap_info_s.bssid[4] = a5 & 0xFF;
                    ap_info_s.bssid[5] = a6 & 0xFF;
                    memcpy(ap_info_p->bssid,ap_info_s.bssid,sizeof(ap_info_s.bssid));
                    continue;
                }
                if (strncmp(line, "ssid=",strlen("ssid=")) == 0 ) {
                    sscanf(line + 5, "%s",ap_info_s.ssid);
                    memcpy(ap_info_p->ssid,ap_info_s.ssid,sizeof(ap_info_s.ssid));
                    continue;
                }
                if (strncmp(line, "passphrase=",strlen("passphrase=")) == 0 ) {
                    sscanf(line + 11, "%s",ap_info_s.passwd);
                    memcpy(ap_info_p->passwd,ap_info_s.passwd,sizeof(ap_info_s.passwd));
                    continue;
                }
                if (strncmp(line, "key_mgmt=",strlen("key_mgmt=")) == 0 ) {
                    sscanf(line + 9, "%s", ap_info_s.key_mgmt);
                    memcpy(ap_info_p->key_mgmt,ap_info_s.key_mgmt,sizeof(ap_info_s.key_mgmt));
                    continue;
                }
                if (strncmp(line, "freq=",strlen("freq=")) == 0 ) {
                    sscanf(line + 5, "%d", &ap_info_s.chan);
                    ap_info_p->chan = ((ap_info_s.chan - 2412) / 5) + 1;
                    continue;
                }

                memcpy(ap_info_p,&ap_info_s,sizeof(WF_CONNECTED_AP_INFO_S));
                return 0;
            }
        }

        break;
    }
    default:
        return -1;
        break;
    }

    return 0;
}

#if 0
static int is_str_gbk(const char* str)
{
    unsigned int nBytes = 0;//GBK可用1-2个字节编�?中文两个 ,英文一�?    unsigned char chr = *str;
    int bAllAscii = 1; //如果全部都是ASCII,
    unsigned int i;
    char chr;

    for (i = 0; str[i] != '\0'; ++i) {
        chr = *(str + i);

        if ((chr & 0x80) != 0 && nBytes == 0) {// 判断是否ASCII编码,如果不是,说明有可能是GBK
            bAllAscii = 0;
        }

        if (nBytes == 0) {
            if (chr >= 0x80) {
                if (chr >= 0x81 && chr <= 0xFE) {
                    nBytes = +2;
                } else {
                    return 0;
                }
                nBytes--;
            }
        } else {
            if (chr < 0x40 || chr>0xFE) {
                return 0;
            }
            nBytes--;
        }//else end
    }

    if (nBytes != 0)  {     //违返规则
        return 0;
    }

    if (bAllAscii){ //如果全部都是ASCII, 也是GBK
        return 1;
    }

    return 1;
}
#endif

static int wifi_send_command(int cmd, void *value, int val_len, void *res, int res_len)
{
    int ret = 0;
    char command[128] = {0};
    struct wifi_network net;
    struct wpa_ctrl *wifi_conn = NULL;
    char *wep_index = NULL;
    char wep_params[35] = {0};
    char wep_command[36] = {0};

    memset(&net, 0, sizeof(net));
    memset(reply, 0, sizeof(reply));
    reply_len = sizeof(reply);

    switch (cmd) {
    case WIFI_COMMAND_SCAN: {
        if (value == NULL) {
            memcpy(command, "SCAN", strlen("SCAN"));
        } else {
            sprintf(command, "SCAN %s", (char *)value);
        }
        break;
    }

    case WIFI_COMMAND_ADDNETWORK: {
        memcpy(command, "ADD_NETWORK", strlen("ADD_NETWORK"));
        break;
    }

    case WIFI_COMMAND_SETNETWORK_SSID :{
        memcpy(&net, (struct wifi_network *)value, val_len);

        sprintf(command, "SET_NETWORK %d ssid \"%s\"", net.id, net.ssid);
        break;
    }

    case WIFI_COMMAND_SETNETWORK_SCANSSID :{
        memcpy(&net, (struct wifi_network *)value, val_len);

        sprintf(command, "SET_NETWORK %d scan_ssid 1",  net.id);
        break;
    }

    case WIFI_COMMAND_SETNETWORK_PSK :{
        memcpy(&net, (struct wifi_network *)value, val_len);

        if (strlen(net.psk) < PSK_WPA_HEX_LEN) {
            sprintf(command, "SET_NETWORK %d psk \"%s\"", net.id, net.psk);
        } else {
            sprintf(command, "SET_NETWORK %d psk %s", net.id, net.psk);
        }
        break;
    }

    case WIFI_COMMAND_SETNETWORK_AUTHALG :{
        strncpy(wep_command, (char *)value, strlen(value));

        memset(wep_params, 0, sizeof(wep_params));
        wep_index = strstr(wep_command, "+");

        strncpy(wep_params, wep_command, wep_index - wep_command);
        net.id = atoi(wep_params);

        memset(wep_params, 0, sizeof(wep_params));
        strncpy(wep_params, wep_index + 1, strlen(wep_command) + wep_command - 1 - wep_index);
        sprintf(command, "SET_NETWORK %d auth_alg %s", net.id, wep_params);

        break;
    }

    case WIFI_COMMAND_SETNETWORK_WEPKEY0 :{
        memcpy(&net, (struct wifi_network *)value, val_len);

        if ((strlen(net.psk) == 5) || (strlen(net.psk) == 13)) {
            sprintf(command, "SET_NETWORK %d wep_key0 \"%s\"", net.id, net.psk);
        } else if ((strlen(net.psk) == 10) || (strlen(net.psk) == 26)) {
            sprintf(command, "SET_NETWORK %d wep_key0 %s", net.id, net.psk);
        }

        break;
    }

    case WIFI_COMMAND_SETNETWORK_WEPTXKEYID :{
        strncpy(wep_command, (char *)value, strlen(value));

        memset(wep_params, 0, sizeof(wep_params));
        wep_index = strstr(wep_command, "+");

        strncpy(wep_params, wep_command, wep_index - wep_command);
        net.id = atoi(wep_params);

        memset(wep_params, 0, sizeof(wep_params));
        strncpy(wep_params, wep_index + 1, strlen(wep_command) + wep_command - 1 - wep_index);

        sprintf(command, "SET_NETWORK %d wep_tx_keyidx %s", net.id, wep_params);

        break;
    }

    case WIFI_COMMAND_SETNETWORK_KEYMETH :{
        memcpy(&net, (struct wifi_network *)value, val_len);

        if ((net.key_mgmt != WIFI_KEY_NONE) && (net.key_mgmt != WIFI_KEY_WEP)) {
            printf("only key mgmt is none or wep need to set key_mgmt \n");
            return -4;
        }

        sprintf(command, "SET_NETWORK %d key_mgmt NONE", net.id);
        break;
    }

    case WIFI_COMMAND_ENABLENETWORK :{
        if (*(int *)value != WPA_ALL_NETWORK) {
            sprintf(command, "ENABLE_NETWORK  %d",  *(int *)value);
        } else {
            sprintf(command, "%s", "ENABLE_NETWORK all");
        }
        break;
    }

    case WIFI_COMMAND_SELECTNETWORK :{
        sprintf(command, "SELECT_NETWORK %d", *(int *)value);
        break;
    }

    case WIFI_COMMAND_REATTACH :{
        sprintf(command, "%s", "REATTACH");
        break;
    }

    case WIFI_COMMAND_RECONFIGURE :{
        sprintf(command, "%s", "RECONFIGURE");
        break;
    }

    case WIFI_COMMAND_GET_SIGNAL:
    case WIFI_COMMAND_GET_CHANNEL:
    {
        sprintf(command, "%s", "SIGNAL_POLL");
        break;
    }

    case WIFI_COMMAND_GET_CURRENT_SSID :
    case WIFI_COMMAND_GET_BSSID :
    case WIFI_COMMAND_GET_STATUS :
    case WIFI_COMMAND_GET_WIFI_INFO:{
        sprintf(command, "%s", "STATUS");
        break;
    }

    case WIFI_COMMAND_SAVECONFIG :{
        sprintf(command, "%s", "SAVE_CONFIG");
        break;
    }

    case WIFI_COMMAND_SCAN_R : {
        sprintf(command, "%s", "SCAN_RESULTS");
        break;
    }

    case WIFI_COMMAND_LISTNETWORK : {
        sprintf(command, "%s", "LIST_NETWORKS");
        break;
    }

    case WIFI_COMMAND_DISABLENETWORK : {
        if (*(int *)value != WPA_ALL_NETWORK) {
            sprintf(command, "DISABLE_NETWORK %d", *(int *)value);
        } else {
            sprintf(command, "%s", "DISABLE_NETWORK all");
        }
        break;
    }

    case WIFI_COMMAND_DISCONNECT : {
        sprintf(command, "%s", "DISCONNECT");
        break;
    }

    case WIFI_COMMAND_RECONNCET : {
        sprintf(command, "%s", "RECONNCET");
        break;
    }

    case WIFI_COMMAND_REMOVENETWORK : {
        if (*(int *)value != WPA_ALL_NETWORK) {
            sprintf(command, "REMOVE_NETWORK %d", *(int *)value);
        } else {
            sprintf(command, "%s", "REMOVE_NETWORK all");
        }
        break;
    }

    case WIFI_COMMAND_BLACKLIST_CLEAR : {
        sprintf(command, "%s", "BLACKLIST clear");
        break;
    }

    case WIFI_COMMAND_SET_COUNTRY: {
        sprintf(command,"set country %s", (char *)value);
        break;
    }

    case WIFI_COMMAND_GET_COUNTRY: {
        sprintf(command,"%s","get country");
        break;
    }

    default:
        break;
    }

    wifi_conn = wpa_ctrl_open(WIFI_WPA_CTRL_PATH);
    if (wifi_conn == NULL) {
        printf("wifi open error\n");
        return -1;
    }

    //printf("wifi request %s \n", command);
    ret = wpa_ctrl_request(wifi_conn, command, strlen(command), reply, &reply_len, NULL);
    if (ret < 0) {
        printf("wifi request error %s \n", command);
    } else {
        ret = wifi_reply_analyse(cmd, reply, strlen(reply), res, res_len);
        if (ret < 0) {
            printf("wifi reply analyse error\n");
        }
    }

    wpa_ctrl_close(wifi_conn);

    return 0;
}

int wifi_join_network(struct wifi_network *net)
{
    int ret = 0;
    int res = 0;
    struct wifi_scan_res scan_r[128];
    struct wifi_scan_list list;
    int i = 0;
    char wep_params[36] = {0};

#if 1
    /* wep
     * hex :: 10 26 32
     * acc :: 5 13 16
     */
    if ((strlen(net->psk) == 5) || (strlen(net->psk) == 13) || (strlen(net->psk) == 16) ||
        (strlen(net->psk) == 10) || (strlen(net->psk) == 26) || (strlen(net->psk) == 32)) {
        memset(&list, 0, sizeof(struct wifi_scan_list));
        list.ssid = &scan_r[0];

        ret = wifi_get_block_scan_result(&list, 3);
        if (ret < 0) {
            printf("wifi_get block scan result error\n");
        }

        for (i = 0; i < list.num; i++) {
            printf("ssid :: %s  psk :: %d\n", list.ssid[i].ssid, list.ssid[i].key_mgmt);
            if (list.ssid[i].key_mgmt == WIFI_KEY_WEP) {
                if (strncmp(list.ssid[i].ssid, net->ssid, strlen(net->ssid)) == 0) {
                    net->key_mgmt = WIFI_KEY_WEP;
                }
            }
        }
    }

    ret = wifi_blacklist_clear();
    if (ret < 0) {
     // printf("cmd id %d  error\n", WIFI_COMMAND_BLACKLIST_CLEAR);
        //return -1;
    }
#endif

    printf("curret network ssid is %s psk %s key %d\n", net->ssid, net->psk, net->key_mgmt);

    ret = wifi_send_command(WIFI_COMMAND_ADDNETWORK, NULL, 0, &net->id, sizeof(net->id));
    if (ret < 0) {
        printf("cmd id %d  error\n", WIFI_COMMAND_ADDNETWORK);
        return -1;
    }

    printf("curret network id is %d\n", net->id);

    ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_SSID, net, sizeof(struct wifi_network), &res, sizeof(res));
    if (ret < 0 || res != 0) {
        printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_SSID, ret, res);
        return -2;
    }

    //set key passwoard or set key mgmt
    if (net->key_mgmt == WIFI_KEY_NONE) {
        ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_KEYMETH, net, sizeof(struct wifi_network), &res, sizeof(res));
        if (ret < 0 || res != 0) {
            printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_KEYMETH, ret, res);
            return -2;
        }
    } else if (net->key_mgmt == WIFI_KEY_WEP) {
        ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_KEYMETH, net, sizeof(struct wifi_network), &res, sizeof(res));
        if (ret < 0 || res != 0) {
            printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_KEYMETH, ret, res);
            return -2;
        }

        memset(wep_params, 0, sizeof(wep_params));
        sprintf(wep_params, "%d+%s", net->id, "SHARED OPEN");

        ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_AUTHALG, wep_params, strlen(wep_params), &res, sizeof(res));
        if (ret < 0 || res != 0) {
            printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_AUTHALG, ret, res);
            return -2;
        }

        ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_WEPKEY0, net, sizeof(struct wifi_network), &res, sizeof(res));
        if (ret < 0 || res != 0) {
            printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_WEPKEY0, ret, res);
            return -2;
        }

        memset(wep_params, 0, sizeof(wep_params));
        sprintf(wep_params, "%d+%s", net->id, "0");

        ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_WEPTXKEYID, wep_params, strlen(wep_params), &res, sizeof(res));
        if (ret < 0 || res != 0) {
            printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_WEPTXKEYID, ret, res);
            return -2;
        }

    } else {
        ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_PSK, net, sizeof(struct wifi_network), &res, sizeof(res));
        if (ret < 0 || res != 0) {
            printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_PSK, ret, res);
            return -2;
        }
    }

    // set scan_ssid 1
    ret = wifi_send_command(WIFI_COMMAND_SETNETWORK_SCANSSID, net, sizeof(struct wifi_network), &res, sizeof(res));
    if (ret < 0 || res != 0) {
        printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SETNETWORK_SCANSSID, ret, res);
        return -2;
    }

    //enable network
    ret = wifi_send_command(WIFI_COMMAND_SELECTNETWORK, &net->id, sizeof(net->id), &res, sizeof(res));
    if (ret < 0 || res != 0) {
        printf("cmd id %d error ret %d res %d\n", WIFI_COMMAND_SELECTNETWORK, ret, res);
        return -2;
    }

    // dhcp_reset();

    return 0;
}

/**
 * dhcp reset, killall dhcpcd, dhcpcd will restart by systemd
 *
 */
void dhcp_reset(void)
{
    system("killall dhcpcd\n");
}

int wifi_save_network(void)
{
    int status = 0;
    int res = 0;
    int ret = 0;

    ret = wifi_enable_all_network();
    if (ret < 0) {
        printf("enable all network error\n");
        return ret;
    }

    if (wifi_get_status(&status) == 0) {
        if (status == WIFI_CONNECTED) {
            wifi_send_command(WIFI_COMMAND_SAVECONFIG, NULL, 0, &res, sizeof(res));
            if (res == 0) {
                printf("save wifi config ok\n");
                return 0;
            }
        }
    }

    return -1;
}

int wifi_get_signal(char *sig)
{
    int ret = 0;
    ret = wifi_send_command(WIFI_COMMAND_GET_SIGNAL, NULL, 0, sig, sizeof(int));
    if (ret < 0) {
        printf("get signal error\n");
        return -1;
    }

    return 0;
}

int wifi_get_current_channel(int *channel)
{
    int ret = 0;
    ret = wifi_send_command(WIFI_COMMAND_GET_CHANNEL, NULL, 0, channel, sizeof(int));
    if (ret < 0) {
        printf("get signal error\n");
        return -1;
    }

    return 0;
}

int wifi_get_status(int *status)
{
    int ret = 0;
    ret = wifi_send_command(WIFI_COMMAND_GET_STATUS, NULL, 0, status, sizeof(int));
    if (ret < 0) {
        printf("get status error\n");
        return -1;
    }

    return 0;
}

int wifi_get_ap_info(WF_CONNECTED_AP_INFO_S *ap_info_s)
{
    int iret = 0;
    WF_CONNECTED_AP_INFO_S ap_info;
    memset(&ap_info,0,sizeof(WF_CONNECTED_AP_INFO_S));

    iret = wifi_send_command(WIFI_COMMAND_GET_WIFI_INFO, NULL, 0, &ap_info, sizeof(WF_CONNECTED_AP_INFO_S));
    if(iret < 0) {
        printf("get connected ap info failed\n");
        return -1;
    }

    memcpy(ap_info_s,&ap_info,sizeof(WF_CONNECTED_AP_INFO_S));

    return 0;
}
// TODO tuya
#if 0
int ping_net_address(char *addr)
{
    char cmd[128] = {0};
    int ret = 0;
    int result = -1;
    CURL *curl;
    CURLcode res;

    sprintf(cmd, "/bin/ping -c 1 -W 3  %s > /dev/null", addr);

    ret = system(cmd);
    if (ret == 0) {
        result = 0;
    } else {
        printf("Do not worry!!! just ping %s error %d errno %d\n", addr, ret, errno);
        curl = curl_easy_init();
        if (!curl) {
            return -1;
        }

        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        curl_easy_setopt(curl, CURLOPT_URL, addr);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        if (res != 0) {
            result = -2;
        } else {
            result = 0;
        }
    }
    return result;
}

int network_get_status(int *status) {
    int ret = 0;

    ret = wifi_get_status(status);
    if (ret < 0) {
        printf("wifi get status error\n");
        return -1;
    }

    if (*status == WIFI_CONNECTED) {
        ret = ping_net_address(SERVER_ADDRESS_PUB_DNS);
        if (ret == 0) {
            *status = NETSERVER_CONNECTED;
        } else {
            ret = ping_net_address(SERVER_ADDRESS);
            if (ret == 0) {
                *status = NETSERVER_CONNECTED;
            } else {
                *status = NETSERVER_UNCONNECTED;
            }
        }
    }

    printf("get network status :: %d\n", *status);

    return 0;
}
#endif

int wifi_scan()
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_SCAN, NULL, 0, &res, sizeof(res));
    if ((ret == 0) && (res == 0)) {
        return 0;
    }

    printf("scan command set error\n");

    return -1;
}


int wifi_scan_channel(int num, int *freq)
{
    int ret = 0;
    int res = 0;
    int i = 0;
    char params[128] = {0};
    char channel[6] = {0};

    if ((freq == NULL) || (num > 21)) {
        return -1;
    }

    if (num == 0) {
        return wifi_scan();
    }

    memcpy(params, "freq=", strlen("freq="));

    for (i = 0; i < num; i++) {
        sprintf(channel, "%d,", freq[i]);

        strcat(params, channel);
    }

    params[strlen(params) - 1] = '\0';

    printf("scan channel :: %s\n", params);

    ret = wifi_send_command(WIFI_COMMAND_SCAN, params, strlen(params), &res, sizeof(res));
    if ((ret == 0) && (res == 0)) {
        return 0;
    }

    printf("scan command set error\n");

    return -1;
}

int wifi_get_block_scan_result(struct wifi_scan_list *list, int timeout)
{
    int ret = 0;
    int status = 0;

    ret = wifi_scan();
    if (ret < 0) {
        printf("wifi scan error not return\n");
    }

    while (1) {
        ret = wifi_get_status(&status);
        if (ret < 0) {
            printf("wifi get status error\n");
            return -1;
        }

        printf("status %d\n", status);

        if ((status != WIFI_SCANING) || (!timeout)) {
            ret = wifi_get_scan_result(list);
            if (ret < 0) {
                printf("wifi get scan result error\n");
                return -1;
            }
            return 0;
        }

        timeout--;
        sleep(1);
    }

    return 0;
}

int wifi_get_scan_result(struct wifi_scan_list *list)
{
    int ret = 0;
    struct wifi_scan_res ssid_list[128];

    if (list == NULL) {
        return -1;
    }

    memset(list, 0, sizeof(struct wifi_scan_list));
    memset(ssid_list, 0, sizeof(ssid_list));

    list->ssid = ssid_list;

    ret = wifi_send_command(WIFI_COMMAND_SCAN_R, NULL, 0, list, sizeof(ssid_list));

    if (ret < 0) {
        printf("wpa get_scan result error\n");
        return -1;
    }

    return 0;
}

int wifi_connect_moni_socket(const char * path)
{
    monitor_conn = wpa_ctrl_open(path);
    if (monitor_conn == NULL) {
        printf("ctrl open monitor socket error\n");
        return -1;
    }
    if (wpa_ctrl_attach(monitor_conn) != 0) {
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
        printf("ctrl attach monitor socket error\n");
        return -1;
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, exit_sockets) == -1) {
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
        printf("ctrl socketpair monitor socket error\n");
        return -1;
    }
    return 0;
}

int wifi_ctrl_recv(char *reply, size_t *reply_len)
{
    // while (1) {
    //     ret = wpa_ctrl_pending(monitor_conn);
    //     if (ret  > 0) {
    //         return wpa_ctrl_recv(monitor_conn, reply, reply_len);

    if (monitor_conn == NULL) {
        printf("monitor is closed\n");
        return -1;
    }

    int res;
    int ctrlfd = wpa_ctrl_get_fd(monitor_conn);
    struct pollfd rfds[2];
    memset(rfds, 0, 2 * sizeof(struct pollfd));
    rfds[0].fd = ctrlfd;
    rfds[0].events |= POLLIN;
    rfds[1].fd = exit_sockets[1];
    rfds[1].events |= POLLIN;

    res = poll(rfds, 2, 6000);
    if (res < 0) {
        printf("Error poll = %d", res);
        return res;
    }

    if (res > 0) {
        if (rfds[0].revents & POLLIN) {
            return wpa_ctrl_recv(monitor_conn, reply, reply_len);
        }
    } else if (res == 0) {
        // timeout
        return 1;
    }
    
    return -2;
}

void wifi_monitor_release()
{
    wpa_ctrl_close(monitor_conn);
    monitor_conn = NULL;
}

int network_get_status(int *status)
{
    return 0;
}

int wifi_get_listnetwork(int *num)
{
    int ret = 0;
    ret = wifi_send_command(WIFI_COMMAND_LISTNETWORK, NULL, 0, num, sizeof(int));
    if (ret < 0) {
        printf("get listnetwork error\n");
        return -1;
    }
    return 0;
}

int wifi_disable_network(int *id)
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_DISABLENETWORK, id, sizeof(int), &res, sizeof(res));
    if (ret < 0 || res != 0) {
        printf("disable network %d  error\n", *id);
        return -1;
    }

    return 0;
}

/**
 * disable all network
 * save config will make all to disabled
 *
 * @return
 */
int wifi_disable_all_network()
{
    int id = WPA_ALL_NETWORK;

    return wifi_disable_network(&id);
}


int wifi_remove_network(int *id)
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_REMOVENETWORK, id, sizeof(int), &res, sizeof(res));
    if (ret < 0 || res != 0) {
        printf("disable network %d  error\n", *id);
        return -1;
    }

    return 0;
}

/**
 * remove all network
 * save config will make all to disabled
 *
 * @return
 */
int wifi_remove_all_network()
{
    int id = WPA_ALL_NETWORK;

    return wifi_remove_network(&id);
}

int wifi_reconfigure()
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_RECONFIGURE, NULL, 0, &res, sizeof(res));
    if ((ret == 0) && (res == 0)) {
        return 0;
    }

    return -1;
}

int wifi_blacklist_clear()
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_BLACKLIST_CLEAR, NULL, 0, &res, sizeof(res));
    if ((ret == 0) && (res == 0)) {
        return 0;
    }

//  printf("blacklist command set error\n");

    return -1;
}

/**
 * enable network
 *
 * @param id  network id
 *
 * @return
 */
int wifi_enable_network(int *id)
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_ENABLENETWORK, id, sizeof(int), &res, sizeof(res));
    if (ret < 0 || res != 0) {
        printf("enable network %d  error\n", *id);
        return -1;
    }

    return 0;
}

/**
 * enbale all network
 *
 *
 * @return
 */
int wifi_enable_all_network()
{
    int id = WPA_ALL_NETWORK;

    return wifi_enable_network(&id);
}

/**
 *
 * disconnect network , wait for reconnect or reassociate
 *
 * @return 0:ok, -1:error
 */
int wifi_disconnect_network()
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_DISCONNECT, NULL, 0, &res, sizeof(res));
    if ((ret == 0) && (res == 0)) {
        return 0;
    }

    return -1;
}

/**
 * 
 * reconnect network, make disconnect to connect
 *
 * @return 0:ok, -1:error
 */
int wifi_reconnect_network()
{
    int ret = 0;
    int res = 0;

    ret = wifi_send_command(WIFI_COMMAND_RECONNCET, NULL, 0, &res, sizeof(res));
    if ((ret == 0) && (res == 0)) {
        return 0;
    }

    return -1;
}
/**
 *
 * goto lowermode mode
 * @param state  ~0 lowermode, 0 normal mode
 *
 * @return
 */
void wifi_goto_lowermode(int state)
{
    if (state) {
        system("iptables -A OUTPUT  -j DROP -o wlan0");
        system("iptables -A INPUT -j DROP -i wlan0");
    } else {
        system("iptables -F");
    }
}
/**
 * get current link ssid
 *
 * @param ssid
 *
 * @return 0
 */
int wifi_get_current_ssid(char *ssid)
{
    int ret = 0;

    memset(ssid, 0, SSID_DEFAULT_LEN);

    ret = wifi_send_command(WIFI_COMMAND_GET_CURRENT_SSID, NULL, 0, ssid, SSID_DEFAULT_LEN);
    if (ret == 0) {
        return 0;
    }

    return -1;
}

/**
 * @brief : get current connected AP's MAC
 *
 * @param [OUT] : ssid
 *
 * @return 0 : success Others : failed
 */
int wifi_get_current_connect_ap_bssid(char *bssid)
{
    int ret = 0;

    memset(bssid, 0,6);

    ret = wifi_send_command(WIFI_COMMAND_GET_BSSID, NULL, 0, bssid, 6);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

int wifi_set_country(char *country)
{
    int ret = 0;
    char res[32] ={0x0};

    ret = wifi_send_command(WIFI_COMMAND_SET_COUNTRY, country, strlen(country), res, sizeof(res));
    if(ret < 0){
        return -1;
    }

    return 0;
}

int wifi_get_country(char *country)
{
    int ret = 0;

    ret = wifi_send_command(WIFI_COMMAND_GET_COUNTRY, NULL, 0, country, strlen(country));
    if(ret < 0){
        return -1;
    }

    return 0;
}

