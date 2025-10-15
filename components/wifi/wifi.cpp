#include <stdio.h>
#include "wifi.h"
#include "esp_event.h" // Event
#include "nvs_flash.h" // NVS storage
#include "Param.h"
#include <string.h>
#include <string>

params::Param<std::string> s_ssid{"wifi_ssid", std::string("")};
params::Param<std::string> s_password{"wifi_password", std::string("")};

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_pm.h"
#include "esp_private/esp_clk.h"
#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"

#define MAX_RECONNECT_TIMES (5)

const static char* TAG               = "WIFI";
static int         s_reconnect_times = 0;

static void wifi_event_any_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                   void* event_data)
{
    ESP_LOGI(TAG, "WiFi event: 0x%" PRIx32, event_id);
}

static void wifi_event_disconnected_handler(void* arg, esp_event_base_t event_base,
                                            int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
    if (s_reconnect_times++ < MAX_RECONNECT_TIMES)
    {
        esp_wifi_connect();
    }
    else
    {
        ESP_LOGE(TAG, "MAX RECONNECT TIME, STOP!");
    }
}

static void wifi_event_connected_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                         void* event_data)
{
    ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
    s_reconnect_times = 0;
}

static void ip_event_got_ip_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                    void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "STA_GOT_IP:" IPSTR, IP2STR(&event->ip_info.ip));
}

static int initialize_wifi(int argc, char** argv)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_any_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                               wifi_event_disconnected_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, wifi_event_connected_handler,
                               NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_got_ip_handler, NULL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* always enable wifi sleep */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_LOGI(TAG, "initialize_wifi DONE.");
    return 0;
}

static void register_wifi_init(void)
{
    const esp_console_cmd_t cmd = {.command  = "wifi_init",
                                   .help     = "Initialize WiFi",
                                   .hint     = NULL,
                                   .func     = &initialize_wifi,
                                   .argtable = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

typedef struct
{
    struct arg_str* ssid;
    struct arg_str* password;
    struct arg_int* authmode;
    struct arg_int* channel;
    struct arg_int* max_conn;
    struct arg_end* end;
} ap_set_args_t;
static ap_set_args_t ap_set_args;

static int cmd_do_ap_set(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &ap_set_args);

    if (nerrors != 0)
    {
        arg_print_errors(stderr, ap_set_args.end, argv[0]);
        return 1;
    }

    wifi_config_t wifi_config = {};

    const char* ssid = ap_set_args.ssid->sval[0];
    strncpy((char*) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    const char* pass = ap_set_args.password->sval[0];
    if (ap_set_args.password->count > 0)
    {
        strncpy((char*) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK; // set default auth mode
    }
    if (ap_set_args.channel->count > 0)
    {
        wifi_config.sta.channel = (uint8_t) (ap_set_args.channel->ival[0]);
    }
    if (ap_set_args.authmode->count > 0)
    {
        wifi_config.ap.authmode = (wifi_auth_mode_t) ap_set_args.authmode->ival[0];
    }
    if (ap_set_args.max_conn->count > 0)
    {
        wifi_config.ap.max_connection = ap_set_args.max_conn->ival[0];
    }
    else
    {
        wifi_config.ap.max_connection = 2;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "set ap config OK.");
    }
    else
    {
        ESP_LOGE(TAG, "set ap config Failed! (%s)", esp_err_to_name(err));
    }
    return 0;
}

static void register_ap_set(void)
{
    ap_set_args.ssid     = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    ap_set_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    ap_set_args.authmode = arg_int0("a", "authmode", "<authmode>",
                                    "wifi auth type (ie. open | wep| wpa2 | wpa2_enterprise)");
    ap_set_args.channel  = arg_int0("n", "channel", "<channel>", "channel of AP");
    ap_set_args.max_conn =
        arg_int0("m", "max_conn", "<max_conn>", "Max station number, default: 2");
    ap_set_args.end             = arg_end(2);
    const esp_console_cmd_t cmd = {.command  = "ap_set",
                                   .help     = "WiFi is ap mode, set ap config.",
                                   .hint     = NULL,
                                   .func     = &cmd_do_ap_set,
                                   .argtable = &ap_set_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

typedef struct
{
    struct arg_str* ssid;
    struct arg_str* password;
    struct arg_int* channel;
    struct arg_end* end;
} sta_connect_args_t;
static sta_connect_args_t connect_args;

static int cmd_do_sta_connect(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &connect_args);

    if (nerrors != 0)
    {
        arg_print_errors(stderr, connect_args.end, argv[0]);
        return 1;
    }

    wifi_config_t wifi_config = {
        .sta =
            {
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            },
    };

    const char* ssid = connect_args.ssid->sval[0];
    memcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    const char* pass = connect_args.password->sval[0];
    if (connect_args.password->count > 0)
    {
        memcpy((char*) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WEP;
    }
    if (connect_args.channel->count > 0)
    {
        wifi_config.sta.channel = (uint8_t) (connect_args.channel->ival[0]);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_LOGI(TAG, "Connecting to %s...", ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    s_reconnect_times = 0;
    esp_err_t err     = esp_wifi_connect();
    ESP_LOGI(TAG, "WIFI_CONNECT_START, ret: 0x%x", err);
    return 0;
}

static void register_sta_connect(void)
{
    connect_args.ssid           = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    connect_args.password       = arg_str0(NULL, NULL, "<pass>", "password of AP");
    connect_args.channel        = arg_int0("n", "channel", "<channel>", "channel of AP");
    connect_args.end            = arg_end(2);
    const esp_console_cmd_t cmd = {.command  = "sta_connect",
                                   .help     = "WiFi is station mode, join specified soft-AP",
                                   .hint     = NULL,
                                   .func     = &cmd_do_sta_connect,
                                   .argtable = &connect_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

typedef struct
{
    struct arg_str* type;
    struct arg_end* end;
} light_sleep_args_t;
static light_sleep_args_t sleep_args;

static int cmd_do_light_sleep(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &sleep_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, sleep_args.end, argv[0]);
        return 1;
    }
    const char* sleep_type = sleep_args.type->sval[0];
    if (!strcmp(sleep_type, "enable"))
    {
        esp_pm_config_t pm_config = {
            .max_freq_mhz       = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
            .min_freq_mhz       = esp_clk_xtal_freq() / 1000000,
            .light_sleep_enable = true,
        };
        ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
        ESP_LOGI(TAG, "LIGHT_SLEEP_ENABLED,OK");
    }
    else
    {
        ESP_LOGE(TAG, "invaild arg!");
        return 1;
    }

    return 0;
}

static void register_light_sleep(void)
{
    sleep_args.type             = arg_str1(NULL, NULL, "<type>", "light sleep mode: enable");
    sleep_args.end              = arg_end(2);
    const esp_console_cmd_t cmd = {
        .command  = "light_sleep",
        .help     = "Config light sleep mode",
        .hint     = NULL,
        .func     = &cmd_do_light_sleep,
        .argtable = &sleep_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

typedef struct
{
    struct arg_str* host;
    struct arg_end* end;
} ping_args_t;
static ping_args_t ping_args;
static void        cmd_ping_on_ping_success(esp_ping_handle_t hdl, void* args)
{
    uint8_t   ttl;
    uint16_t  seqno;
    uint32_t  elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%lu bytes from %s icmp_seq=%u ttl=%u time=%lu ms", recv_len,
             ipaddr_ntoa((ip_addr_t*) &target_addr), seqno, ttl, elapsed_time);
}
static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void* args)
{
    uint16_t  seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGI(TAG, "From %s icmp_seq=%d timeout", ipaddr_ntoa((ip_addr_t*) &target_addr), seqno);
}
static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void* args)
{
    ip_addr_t target_addr;
    uint32_t  transmitted;
    uint32_t  received;
    uint32_t  total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    uint32_t loss = (uint32_t) ((1 - ((float) received) / transmitted) * 100);
    ESP_LOGI(TAG, "\n--- %s ping statistics ---", inet6_ntoa(*ip_2_ip6(&target_addr)));
    ESP_LOGI(TAG, "%lu packets transmitted, %lu received, %lu%% packet loss, time %lums",
             transmitted, received, loss, total_time_ms);
    // delete the ping sessions, so that we clean up all resources and can create a new ping session
    // we don't have to call delete function in the callback, instead we can call delete function from other tasks
    esp_ping_delete_session(hdl);
}
static int do_ping_cmd(int argc, char** argv)
{
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.task_stack_size   = 4096;
    ip_addr_t target_addr    = {0};

    int nerrors = arg_parse(argc, argv, (void**) &ping_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, ping_args.end, argv[0]);
        return 1;
    }

    // if (!g_nan_netif)
    // {
    //     ESP_LOGE(TAG, "NAN not started successfully");
    //     return 1;
    // }

    if (ping_args.host->count)
    {
        /* convert ip6 string to ip6 address */
        ipaddr_aton(ping_args.host->sval[0], &target_addr);
    }
    else
    {
        ESP_LOGE(TAG, "No Active datapath for ping");
        return 1;
    }

    config.target_addr = target_addr;
    // config.interface   = esp_netif_get_netif_impl_index(g_nan_netif);

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end     = cmd_ping_on_ping_end,
    };
    esp_ping_handle_t ping;
    if (esp_ping_new_session(&config, &cbs, &ping) == ESP_OK)
    {
        ESP_LOGI(TAG, "Pinging Peer with IPv6 addr %s", ipaddr_ntoa((ip_addr_t*) &target_addr));
        esp_ping_start(ping);
        return 0;
    }
    else
    {
        ESP_LOGI(TAG, "Failed to ping Peer with IPv6 addr %s",
                 ipaddr_ntoa((ip_addr_t*) &target_addr));
        return 1;
    }
    return 0;
}
void register_ping_cmd(void)
{
    ping_args.host = arg_str1(NULL, NULL, "<host>", "Host address");
    ping_args.end  = arg_end(1);

    const esp_console_cmd_t ping_cmd = {.command  = "ping",
                                        .help     = "send ICMP ECHO_REQUEST to network hosts",
                                        .hint     = NULL,
                                        .func     = &do_ping_cmd,
                                        .argtable = &ping_args};

    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));
}

extern void register_wifi_cmd(void)
{
    register_wifi_init();
    register_ap_set();
    register_sta_connect();
    register_light_sleep();
    register_ping_cmd();
}

void espwifi_Init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    register_wifi_cmd();
}
