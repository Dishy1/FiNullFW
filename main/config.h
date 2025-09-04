#pragma once
void config_init(void);
const char *config_get_hwid(void);
const char *config_get_expiry(void);
void config_set_hwid(const char *hwid);
void config_set_expiry(const char *expiry);