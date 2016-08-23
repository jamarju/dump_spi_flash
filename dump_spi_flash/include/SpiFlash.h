#pragma once

#include <stdbool.h>

void spi_flash_init(void);
void spi_flash_led(bool b);
void spi_flash_read_id(void);
void spi_flash_dump(void);
void spi_flash_upload(void);
void spi_flash_verify(void);


