/* MDNS-SD Query and advertise Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_ip_addr.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_netif.h"
#include "mdns.h"
#include "netdb.h"


#define EXAMPLE_MDNS_INSTANCE CONFIG_MDNS_INSTANCE
#define EXAMPLE_BUTTON_GPIO     5

static const char * TAG = "mdns-test";



/* these strings match tcpip_adapter_if_t enumeration */
static const char * if_str[] = {"STA", "AP", "ETH", "MAX"};

/* these strings match mdns_ip_protocol_t enumeration */
static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};

void initialise_mdns(void){


    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
   // query_mdns_service("_snapcast", "_tcp");
}





void mdns_print_results(mdns_result_t *results)
{
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;
    while (r) {
        printf("%d: Interface: %s, Type: %s, TTL: %u\n", i++, if_str[r->tcpip_if], ip_protocol_str[r->ip_protocol],
               r->ttl);
        if (r->instance_name) {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%zu] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }
}

void query_mdns_service(const char * service_name, const char * proto)
{
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t * results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if(err){
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }
    if(!results){
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
}

bool check_and_print_result(mdns_search_once_t *search)
{
    // Check if any result is available
    mdns_result_t * result = NULL;
    if (!mdns_query_async_get_results(search, 0, &result)) {
        return false;
    }

    if (!result) {   // search timeout, but no result
        return true;
    }

    // If yes, print the result
    mdns_ip_addr_t * a = result->addr;
    while (a) {
        if(a->addr.type == ESP_IPADDR_TYPE_V6){
            printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
        } else {
            printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
        }
        a = a->next;
    }
    // and free the result
    mdns_query_results_free(result);
    return true;
}

void query_mdns_hosts_async(const char * host_name)
{
    ESP_LOGI(TAG, "Query both A and AAA: %s.local", host_name);

    mdns_search_once_t *s_a = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_A, 1000, 1, NULL);
    mdns_query_async_delete(s_a);
    mdns_search_once_t *s_aaaa = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
    while (s_a || s_aaaa) {
        if (s_a && check_and_print_result(s_a)) {
            ESP_LOGI(TAG, "Query A %s.local finished", host_name);
            mdns_query_async_delete(s_a);
            s_a = NULL;
        }
        if (s_aaaa && check_and_print_result(s_aaaa)) {
            ESP_LOGI(TAG, "Query AAAA %s.local finished", host_name);
            mdns_query_async_delete(s_aaaa);
            s_aaaa = NULL;
        }
    }
}

void query_mdns_host(const char * host_name)
{
    ESP_LOGI(TAG, "Query A: %s.local", host_name);

    struct esp_ip4_addr addr;
    addr.addr = 0;

    esp_err_t err = mdns_query_a(host_name, 2000,  &addr);
    if(err){
        if(err == ESP_ERR_NOT_FOUND){
            ESP_LOGW(TAG, "%s: Host was not found!", esp_err_to_name(err));
            return;
        }
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Query A: %s.local resolved to: " IPSTR, host_name, IP2STR(&addr));
}






