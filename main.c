#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "asm.pio.h"

#define PIO_IRIS_SM 1
#define PIO_ID pio1
#define DATA 0x0100
#define COMMAND 0x0000
static void init_iris(void);
static void init_dma(void);
int dma_channel;
#define HEADER_LEN 1
#define LCD_WIDTH 128
#define LCD_HEIGHT 132

uint16_t header[HEADER_LEN] = { 
    // Write
    COMMAND | 0x2C,
};

#define DMA_LEN (HEADER_LEN+2*LCD_WIDTH*LCD_HEIGHT)
#define SECTOR_SIZE 4096

uint16_t dma_data[DMA_LEN*2];

uint32_t sector = 0;
uint32_t pos = 0;
uint8_t video_cache[SECTOR_SIZE];

void reset_lcd_data() {
    memset(dma_data, DATA>>8, sizeof(dma_data));
    memcpy(dma_data, header, sizeof(header));
    memcpy(dma_data+DMA_LEN, header, sizeof(header));
}

#define MULTICORE 0
#define DMA 1
#define PIXELS (128*96)

void core1_entry(void) {
    bool color = false;
    uint8_t count;
    uint16_t frame_remain = 6571;
    
    uint32_t data_pos = 0;
    uint16_t *dma_head;
    uint16_t dma_pos = 1;
    uint8_t buf[SECTOR_SIZE];
    uint32_t sector_index = 0;
#if MULTICORE
    uint32_t released_id;
    multicore_fifo_push_blocking(1);
    released_id = multicore_fifo_pop_blocking() & 1;
    dma_head = dma_data+released_id*DMA_LEN;
#else
    dma_head = dma_data;
#endif
    
    while(true) {
        memcpy(buf, (uint8_t*)XIP_BASE+0x10000+SECTOR_SIZE*sector_index++, SECTOR_SIZE);
        data_pos = 0;
        while(data_pos<4096) {
            count = buf[data_pos++];
            while(count) {
                dma_head[dma_pos++] = color ? 0x1FF : 0x100;
                dma_head[dma_pos++] = color ? 0x1FF : 0x100;
                count--;
            }
            color = !color;
            if( dma_pos>=1+PIXELS*2 ) {
#if MULTICORE
                multicore_fifo_push_blocking(released_id & 1);
                released_id = multicore_fifo_pop_blocking() & 1;
                dma_head = dma_data+released_id*DMA_LEN;
#else
                gpio_put(25, true);
#if DMA
                while( dma_channel_is_busy(dma_channel) );
                dma_channel_set_read_addr(dma_channel, dma_head, true);
#else
                for(uint16_t i=0; i<1+2*PIXELS; i++) {
                    pio_sm_put_blocking(PIO_ID, PIO_IRIS_SM, dma_head[i]);
                }
#endif
                gpio_put(25, false);
#endif
                dma_pos = 1;
                frame_remain--;
                color = false;
                if(! frame_remain ) {
                    return;
                }
            }
        }
    }
        
}

int main()
{
    stdio_init_all();
    set_sys_clock_khz(250000, true);
    reset_lcd_data();
    
    multicore_fifo_push_blocking(0);
    
    init_iris();
    init_dma();
    
    gpio_init(25);
    gpio_set_dir(25, true);
    
    multicore_launch_core1(core1_entry);
#if MULTICORE
    while(true) {
        uint32_t write_id = multicore_fifo_pop_blocking() & 1;

        gpio_put(25, true);
        for(uint16_t i=0; i<1+2*PIXELS; i++) {
            pio_sm_put_blocking(PIO_ID, PIO_IRIS_SM, dma_data[i+write_id*DMA_LEN]);
        }
        gpio_put(25, false);
        
        multicore_fifo_push_blocking(write_id & 1);
    }
#else
    while(true) {
        __wfe();
    }
#endif
}

#define INIT_DATA_LEN 85

uint16_t init_data[INIT_DATA_LEN] = { //Initialize code
	//(from https://github.com/kingyoPiyo/TFT_Test_48x640/blob/main/TFT_Test/TFT_Test.ino)
	//using ILI9342C TFT driver IC.

	// 4
	COMMAND | 0xEF, DATA | 0x03, DATA | 0x80, DATA | 0x92,
	// 4
	COMMAND | 0xCF, DATA | 0x00, DATA | 0xC1, DATA | 0x30,
	// 5
	COMMAND | 0xED, DATA | 0x64, DATA | 0x03, DATA | 0x12, DATA | 0x81,
	// 4
	COMMAND | 0xE8, DATA | 0x85, DATA | 0x00, DATA | 0x78,
	// 2
	COMMAND | 0xF7, DATA | 0x20,
	// 3
	COMMAND | 0xEA, DATA | 0x00, DATA | 0x00,
	// 5 Column Address Set
	COMMAND | 0x2A, DATA | 0x00, DATA | 0x00, DATA | 0x00, DATA | 0x7F /*+2*/,
	// 5 Page Address Set
	COMMAND | 0x2B, DATA | 0x00, DATA | 0x00, DATA | 0x00, DATA | 0x83,
	// 2 Memory Access Control
	COMMAND | 0x36, DATA | 0xC8,
	// 2 Power Control 1
	COMMAND | 0xC0, DATA | 0x23,
	// 2 Power Control 2
	COMMAND | 0xC1, DATA | 0x10,
	// 3 VCOM Control
	COMMAND | 0xC5, DATA | 0x3E, DATA | 0x28,
	// 2
	COMMAND | 0xC7, DATA | 0x86,
	// 2 Pixel Format Set
	COMMAND | 0x3A, DATA | 0x55,
	// 3 Frame Rate Control
	COMMAND | 0xB1, DATA | 0x00, DATA | 0x13,
	// 2
	COMMAND | 0xF2, DATA | 0x00,
	// 2
	COMMAND | 0x26, DATA | 0x01,
	// 16 Positive Gamma Correction
	COMMAND | 0xE0, DATA | 0x00, DATA | 0x1C, DATA | 0x21, DATA | 0x02, DATA | 0x11, DATA | 0x07, DATA | 0x3D, DATA | 0x79, DATA | 0x4B, DATA | 0x07, DATA | 0x0F, DATA | 0x0C, DATA | 0x1B, DATA | 0x1F, DATA | 0x0F,
	// 16 Negative Gamma Correction
	COMMAND | 0xE1, DATA | 0x00, DATA | 0x1C, DATA | 0x20, DATA | 0x04, DATA | 0x0F, DATA | 0x04, DATA | 0x33, DATA | 0x45, DATA | 0x42, DATA | 0x04, DATA | 0x0C, DATA | 0x0A, DATA | 0x22, DATA | 0x29, DATA | 0x0F,
	// 1 Sleep Out
	COMMAND | 0x11,
};

void reset_iris(void) {
    gpio_put(14, false); 
	sleep_ms(100);
	gpio_put(14, true);
	sleep_ms(100);
	
	for(uint16_t i=0; i<INIT_DATA_LEN; i++) {
	    pio_sm_put_blocking(PIO_ID, PIO_IRIS_SM, init_data[i]);
	}
	sleep_ms(130);//greater than 120ms
	
	pio_sm_put_blocking(PIO_ID, PIO_IRIS_SM, COMMAND | 0x29);
    sleep_ms(10);
}

static void init_iris(void) {
    // send program
    pio_iris_program_init(PIO_ID, PIO_IRIS_SM);
	
	//Reset display
	gpio_init(14);
	gpio_set_dir(14, true); // set output
	reset_iris();
}

static void init_dma(void) {
    dma_channel = dma_claim_unused_channel(true);
    
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO1_TX1); // DREQ_PIO0_TX0
    
    dma_channel_configure(
        dma_channel,          // Channel to be configured
        &c,            // The configuration we just created
        PIO_ID->txf+1,           // The initial write address
        dma_data,           // The initial read address
        DMA_LEN, // Number of transfers; in this case each is 2 byte.
        false           // Start immediately.
    );
}

