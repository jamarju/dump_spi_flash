#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>

#include "stm32f10x.h"

#include "SpiFlash.h"

/*********************************************************************
 * Constants
 *********************************************************************/

#define FLASH_SIZE					0x800000	// Bytes
#define BUF_SIZE 					0x4000		// Bytes
#define PAGE_SIZE					256			// Bytes

#define CMD_ID 						0x9f	// in: - out: manufacturer, mem type, capacity
#define CMD_READ_STATUS_REG_1 		0x05	// in: - out: S7-S0
#define CMD_READ_STATUS_REG_2		0x35	// in: - out: S15-S8
#define CMD_READ_DATA				0x03	// in: A23-A16, A15-A8, A7-A0 out: D7-D0
#define CMD_WRITE_ENABLE			0x06	// in: - out: -
#define CMD_WRITE_DISABLE			0x04	// in: - out: -
#define CMD_CHIP_ERASE				0xC7	// in: - out: -
#define CMD_PAGE_PROGRAM			0x02	// in: A23-A16, A15-A8, A7-A0, data0, [..., data255] out: -

// Status register bits
#define SR1_BUSY					0
#define SR1_WEL						1
#define SR1_BP0						2
#define SR1_BP1						3
#define SR1_BP2						4
#define SR1_TB						5
#define SR1_SEC						6
#define SR1_SRP0					7


/*********************************************************************
 * Local vars
 *********************************************************************/

/* Some benchmarks dumping 64 Mb of flash (= 8 MB):
 *
 * fopen/fwrite/fclose, BUF_SIZE = 0x1000 -> 7270 B/s
 * open/write/close,    BUF_SIZE = 0x1000 -> 21708 B/s
 * open/write/close,    BUF_SIZE = 0x2000 -> 33587 B/s
 * open/write/close,    BUF_SIZE = 0x4000 -> 44236 B/s
 * o/w/c + printf %,	BUF_SIZE = 0x4000 -> 32786 B/s
 *
 * SPI clock doesn't seem to affect much since the bottleneck is in the
 * semihosting overhead.
 */
static uint8_t rx_buffer[BUF_SIZE];



/*********************************************************************
 * Local functions
 *********************************************************************/

static void
spi_flash_select(bool b)
{
	if (b) {
		GPIO_ResetBits(GPIOB, GPIO_Pin_12);
	} else {
		GPIO_SetBits(GPIOB, GPIO_Pin_12);
	}
}

static void
spi_flash_send(uint8_t *tx_buf, int tx_buf_len)
{
	int i;

	// Send
	for (i = 0; i < tx_buf_len; i++) {
		while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
		SPI_I2S_SendData(SPI1, tx_buf[i]);

		// Since we are full duplex we must read the incoming (dummy) bytes
		while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
		(void)SPI_I2S_ReceiveData(SPI1);
	}
}

static void
spi_flash_receive(uint8_t *rx_buf, int rx_buf_len)
{
	int i;

	// Receive
	for (i = 0; i < rx_buf_len; i++) {
		while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
		SPI_I2S_SendData(SPI1, (0x00));                                             // send Dummy byte
		while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
		while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
		rx_buf[i] = SPI_I2S_ReceiveData(SPI1);
	}
}


static void
spi_flash_debug(uint8_t *tx_buf, int tx_buf_len, uint8_t *rx_buf, int rx_buf_len)
{
	int i;
	printf("->");
	for (i = 0; i < tx_buf_len; i++) {
		printf(" %02x", tx_buf[i]);
	}
	printf("\r\n<-");
	for (i = 0; i < rx_buf_len; i++) {
		printf(" %02x", rx_buf[i]);
	}
	printf("\r\n");
}


static void
spi_flash_send_cmd(uint8_t *tx_buf, int tx_buf_len, uint8_t *rx_buf, int rx_buf_len)
{
	// Select device
	spi_flash_select(true);

	spi_flash_send(tx_buf, tx_buf_len);
	spi_flash_receive(rx_buf, rx_buf_len);

	// Wait while busy
	while( SPI1->SR & SPI_I2S_FLAG_BSY );

	// Deselect
	spi_flash_select(false);
}

static void
spi_flash_wait_while_busy()
{
	uint8_t cmd = CMD_READ_STATUS_REG_1;
	uint8_t res;

	spi_flash_select(true);
	spi_flash_send(&cmd, 1);

	do {
		spi_flash_receive(&res, 1);
	} while (res & (1 << SR1_BUSY));

	// Wait while busy
	while( SPI1->SR & SPI_I2S_FLAG_BSY );

	spi_flash_select(false);
}

static void
spi_flash_write_enable(void)
{
	uint8_t cmd = CMD_WRITE_ENABLE;
	spi_flash_send_cmd(&cmd, 1, NULL, 0);
}

/*********************************************************************1
 * Exported functions
 *********************************************************************/

void spi_flash_init(void)
{
	// Enable GPIO Peripheral clock
	RCC_APB2PeriphClockCmd(
			RCC_APB2Periph_GPIOA |
			RCC_APB2Periph_GPIOB |
			RCC_APB2Periph_GPIOC |
			RCC_APB2Periph_SPI1,
			ENABLE);

	// GPIO init
	GPIO_InitTypeDef GPIO_InitStructure;

	// A5 (CLK), A7 (MOSI) -> Alternate function push/pull out
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// A6 (MISO) -> Nopull input
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// B12 (/CS) -> push/pull output
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// C13 (LED) in output push/pull mode
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// SPI Init
	SPI_InitTypeDef SPI_InitStructure;

	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128; // 72 / prescaler MHz
	SPI_InitStructure.SPI_CPHA 				= SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_CPOL 				= SPI_CPOL_Low;
	SPI_InitStructure.SPI_CRCPolynomial 	= 7;
	SPI_InitStructure.SPI_DataSize 			= SPI_DataSize_8b;
	SPI_InitStructure.SPI_Direction 		= SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_FirstBit 			= SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_Mode 				= SPI_Mode_Master;
	SPI_InitStructure.SPI_NSS 				= SPI_NSS_Soft;
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, ENABLE);

	spi_flash_select(false);
}


void
spi_flash_led(bool b)
{
	if (b) {
		GPIO_ResetBits(GPIOC, GPIO_Pin_13);
	} else {
		GPIO_SetBits(GPIOC, GPIO_Pin_13);
	}
}


void spi_flash_dump(void)
{
	uint8_t cmd[] = { CMD_READ_DATA, 0, 0, 0 };
	int addr;
	//FILE *f;
	int h;

	puts("* Read flash\r\n");

	// Open file in host
	//f = fopen("dump.bin", "wb");
	h = open("dump.bin", O_CREAT|O_WRONLY|O_TRUNC);

	// Select device
	spi_flash_select(true);

	// Send read cmd
	spi_flash_send(cmd, sizeof(cmd));

	for (addr = 0; addr < FLASH_SIZE; addr += BUF_SIZE) {
		spi_flash_led(true);
		spi_flash_receive(rx_buffer, sizeof(rx_buffer));
		spi_flash_led(false);
		//fwrite(rx_buffer, 1, 0x1000, f);
		write(h, rx_buffer, BUF_SIZE);
		printf("%d%%\r", addr * 100 / FLASH_SIZE);
		fflush(stdout);
	}

	//fclose(f);
	close(h);
	puts("100%\n");
}


void spi_flash_read_id(void)
{
	uint8_t cmd;
	uint8_t res[3];

	puts("* Read id\r\n");
	cmd = CMD_ID;
	spi_flash_send_cmd(&cmd, 1, res, 3);
	spi_flash_debug(&cmd, 1, res, 3);

	puts("* Read status reg 1\r\n");
	cmd = CMD_READ_STATUS_REG_1;
	spi_flash_send_cmd(&cmd, 1, res, 1);
	spi_flash_debug(&cmd, 1, res, 1);

	puts("* Read status reg 2\r\n");
	cmd = CMD_READ_STATUS_REG_2;
	spi_flash_send_cmd(&cmd, 1, res, 1);
	spi_flash_debug(&cmd, 1, res, 1);
}

void spi_flash_upload(void)
{
	uint8_t cmd[4];
	int h;
	int addr;

	puts("* Write enable\r\n");
	spi_flash_write_enable();

	puts("* Chip erase\r\n");
	cmd[0] = CMD_CHIP_ERASE;
	spi_flash_send_cmd(cmd, 1, NULL, 0);
	spi_flash_debug(cmd, 1, NULL, 0);

	spi_flash_wait_while_busy();

	puts("* Write flash\r\n");

	// Open file in host
	h = open("dump.bin", O_RDONLY);
	if (h == -1) {
		puts("ERROR: a dump.bin must exist in openocd's cwd\r\n");
		exit(1);
	}

	for (addr = 0; addr < FLASH_SIZE; addr += BUF_SIZE) {
		// Turn on led
		spi_flash_led(true);

		// Read page from host
		if (read(h, rx_buffer, BUF_SIZE) != BUF_SIZE) {
			puts("ERROR: dump.bin must be 8 MB long\r\n");
			exit(1);
		}

		// Turn off led
		spi_flash_led(false);

		int page_addr;
		for (page_addr = 0; page_addr < BUF_SIZE; page_addr += PAGE_SIZE) {

			// Write enable
			spi_flash_write_enable();

			// Select device
			spi_flash_select(true);

			// Send page program cmd
			cmd[0] = CMD_PAGE_PROGRAM;
			cmd[1] = (uint8_t) ((addr + page_addr) >> 16);
			cmd[2] = (uint8_t) ((addr + page_addr) >> 8);
			cmd[3] = 0;
			spi_flash_send(cmd, 4);

			// Send page and deselect to end command
			spi_flash_send(rx_buffer + page_addr, PAGE_SIZE);

			// Wait while busy
			while( SPI1->SR & SPI_I2S_FLAG_BSY );

			// End command
			spi_flash_select(false);

			// Wait until ready
			spi_flash_wait_while_busy();
		}

		if (addr % 0x10000 == 0) {
			printf("%d%%\r", addr * 100 / FLASH_SIZE);
			fflush(stdout);
		}
	}

	close(h);
}

void spi_flash_verify(void)
{
	uint8_t cmd[] = { CMD_READ_DATA, 0, 0, 0 };
	int addr;
	//FILE *f;
	int h;

	puts("* Verify flash\r\n");

	// Open file in host
	h = open("dump.bin", O_RDONLY);
	if (h == -1) {
		puts("ERROR: a dump.bin must exist in openocd's cwd\r\n");
		exit(1);
	}

	// Select device
	spi_flash_select(true);

	// Send read cmd
	spi_flash_send(cmd, sizeof(cmd));

	for (addr = 0; addr < FLASH_SIZE; addr += BUF_SIZE / 2) {
		spi_flash_led(true);
		spi_flash_receive(rx_buffer, BUF_SIZE / 2);
		spi_flash_led(false);

		read(h, rx_buffer + BUF_SIZE / 2, BUF_SIZE / 2);

		int i;
		for (i = 0; i < BUF_SIZE / 2 && rx_buffer[i] == rx_buffer[i + BUF_SIZE / 2]; i++) ;
		if (i != BUF_SIZE / 2) {
			printf("ERROR: mismatch at addr 0x%x (0x%x should be 0x%x)\r\n", addr + i, rx_buffer[i], rx_buffer[i + BUF_SIZE / 2]);
			close(h);
			return;
		}

		if (addr % 0x10000 == 0) {
			printf("%d%%\r", addr * 100 / FLASH_SIZE);
			fflush(stdout);
		}
	}

	close(h);
	puts("100% verify OK!!\r\n");
}
