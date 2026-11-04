#pragma once
#include <stdint.h>
constexpr int ESP_EAP_TYPE_PEAP=2;
int esp_eap_client_set_eap_methods(int);
int esp_eap_client_set_disable_time_check(bool);
int esp_eap_client_set_identity(const uint8_t*,int);
int esp_eap_client_set_username(const uint8_t*,int);
int esp_eap_client_set_password(const uint8_t*,int);
int esp_eap_client_set_domain_name(const char*);
int esp_eap_client_use_default_cert_bundle(bool);
int esp_eap_client_set_ca_cert(const uint8_t*,int);
void esp_eap_client_clear_identity();
void esp_eap_client_clear_username();
void esp_eap_client_clear_password();
void esp_eap_client_clear_new_password();
void esp_eap_client_clear_ca_cert();
void esp_eap_client_clear_certificate_and_key();
