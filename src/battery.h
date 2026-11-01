#pragma once

void battery_init(void);
float battery_voltage(void); // média de várias leituras
int battery_percent(float v);
