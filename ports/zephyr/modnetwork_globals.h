
#if MICROPY_PY_NETWORK_WLAN
{ MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&zephyr_network_initialize_obj) },
{ MP_ROM_QSTR(MP_QSTR_WLAN), MP_ROM_PTR(&zephyr_network_wlan_type) },
// Constants
{ MP_ROM_QSTR(MP_QSTR_STA_IF), MP_ROM_INT(MOD_NETWORK_STA_IF) },
{ MP_ROM_QSTR(MP_QSTR_AP_IF), MP_ROM_INT(MOD_NETWORK_AP_IF) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_OPEN), MP_ROM_INT(WIFI_SECURITY_TYPE_NONE)},
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA2_PSK), MP_ROM_INT(WIFI_SECURITY_TYPE_PSK)},
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA2_PSK_SHA256), MP_ROM_INT(WIFI_SECURITY_TYPE_PSK_SHA256)},
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA3_SAE), MP_ROM_INT(WIFI_SECURITY_TYPE_SAE)},
{ MP_ROM_QSTR(MP_QSTR_MFP_DISABLE), MP_ROM_INT(WIFI_MFP_DISABLE)},
{ MP_ROM_QSTR(MP_QSTR_MFP_OPTIONAL), MP_ROM_INT(WIFI_MFP_OPTIONAL)},
{ MP_ROM_QSTR(MP_QSTR_MFP_REQUIRED), MP_ROM_INT(WIFI_MFP_REQUIRED)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_BAND_2_4_GHZ), MP_ROM_INT(WIFI_FREQ_BAND_2_4_GHZ)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_BAND_5_GHZ), MP_ROM_INT(WIFI_FREQ_BAND_5_GHZ)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_BAND_6_GHZ), MP_ROM_INT(WIFI_FREQ_BAND_6_GHZ)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_BAND_ALL), MP_ROM_INT(__WIFI_FREQ_BAND_AFTER_LAST)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_MODE_INFRA), MP_ROM_INT(WIFI_MODE_INFRA)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_MODE_IBSS), MP_ROM_INT(WIFI_MODE_IBSS)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_MODE_AP), MP_ROM_INT(WIFI_MODE_AP)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_MODE_P2P_GO), MP_ROM_INT(WIFI_MODE_P2P_GO)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_MODE_P2P_GROUP_FORMATION), MP_ROM_INT(WIFI_MODE_P2P_GROUP_FORMATION)},
{ MP_ROM_QSTR(MP_QSTR_WIFI_MODE_MESH), MP_ROM_INT(WIFI_MODE_MESH)},
{ MP_ROM_QSTR(MP_QSTR_STAT_IDLE), MP_ROM_INT(WIFI_STATE_DISCONNECTED)},
{ MP_ROM_QSTR(MP_QSTR_STAT_CONNECTING), MP_ROM_INT(WIFI_STATE_AUTHENTICATING)},
{ MP_ROM_QSTR(MP_QSTR_STAT_GOT_IP), MP_ROM_INT(WIFI_STATE_COMPLETED)},
#endif // MICROPY_PY_NETWORK_WLAN

#if MICROPY_PY_NETWORK_NRF91
{ MP_ROM_QSTR(MP_QSTR_CELL), MP_ROM_PTR(&zephyr_network_cell_type) },
// Constants
{ MP_ROM_QSTR(MP_QSTR_LTE_MODE_LTEM), MP_ROM_INT(LTE_MODE_LTEM) },
{ MP_ROM_QSTR(MP_QSTR_LTE_MODE_NBIOT), MP_ROM_INT(LTE_MODE_NBIOT) },
{ MP_ROM_QSTR(MP_QSTR_LTE_MODE_GPS), MP_ROM_INT(LTE_MODE_GPS) },
{ MP_ROM_QSTR(MP_QSTR_PLMN_PREF), MP_ROM_INT(LTE_PLMN_PREF) },
#ifdef CONFIG_PDN
{ MP_ROM_QSTR(MP_QSTR_PDN_FAM_IPV4), MP_ROM_INT(PDN_FAM_IPV4) },
{ MP_ROM_QSTR(MP_QSTR_PDN_FAM_IPV6), MP_ROM_INT(PDN_FAM_IPV6) },
{ MP_ROM_QSTR(MP_QSTR_PDN_FAM_IPV4V6), MP_ROM_INT(PDN_FAM_IPV4V6) },
{ MP_ROM_QSTR(MP_QSTR_PDN_FAM_NONIP), MP_ROM_INT(PDN_FAM_NONIP) },
{ MP_ROM_QSTR(MP_QSTR_PDN_AUTH_PAP), MP_ROM_INT(PDN_AUTH_PAP) },
{ MP_ROM_QSTR(MP_QSTR_PDN_AUTH_CHAP), MP_ROM_INT(PDN_AUTH_CHAP) },
#endif
#endif
