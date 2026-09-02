/**
 * @file spi_mcu.c
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

/*==================[inclusions]=============================================*/
#include "spi_mcu.h"
#include <stdint.h>
#include <string.h>
#include "driver/spi_master.h"
#include "gpio_mcu.h"
/*==================[macros and definitions]=================================*/
#define PIN_NUM_MOSI	GPIO_11	/*!<  */
#define PIN_NUM_CLK		GPIO_12	/*!<  */
#define PIN_NUM_CS1		GPIO_10	/*!<  */
/*==================[internal data declaration]==============================*/
spi_device_handle_t spi_1;
const spi_bus_config_t bus_cfg = {
    .miso_io_num = -1,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4092
};
transfer_mode_t transfer_mode_1;
void (*spi_1_isr_p)(void*);	/*!<  */
void *spi_1_user_data;	    /*!<  */
/*==================[internal functions declaration]=========================*/
static void spi_1_isr(spi_transaction_t *t){
	spi_1_isr_p(spi_1_user_data);
}
/*==================[internal data definition]===============================*/

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

/*==================[external functions definition]==========================*/
uint8_t SpiInit(spi_mcu_config_t* spi){
    static bool spi_initialized = false;
    if(!spi_initialized){
	    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
        spi_initialized = true;
    }
	spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = spi->bitrate,     	
        .mode = spi->clk_mode,                  
        .queue_size = 8,                        
    };
    dev_cfg.spics_io_num = PIN_NUM_CS1;
    transfer_mode_1 = spi->transfer_mode;
    if(transfer_mode_1 == SPI_INTERRUPT){
        dev_cfg.post_cb = spi_1_isr;
    }
    spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_1);
    spi_1_isr_p = spi->func_p;
    spi_1_user_data = spi->param_p;
    return 0;
}

void SpiRead(spi_dev_t device, uint8_t * rx_buffer, uint32_t rx_buffer_size){
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length = rx_buffer_size * 8;  // tx_buffer_size is in bytes, transaction length is in bits.
    t.rxlength = rx_buffer_size * 8;
    t.rx_buffer = rx_buffer;        // Data
    switch(transfer_mode_1){
        case SPI_POLLING:
            spi_device_polling_transmit(spi_1, &t);
            break;
        case SPI_INTERRUPT:
            spi_device_transmit(spi_1, &t);
            break;
    }
}

void SpiWrite(spi_dev_t device, uint8_t * tx_buffer, uint32_t tx_buffer_size){
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length = tx_buffer_size * 8;  // tx_buffer_size is in bytes, transaction length is in bits.
    t.tx_buffer = tx_buffer;        // Data
    switch(transfer_mode_1){
        case SPI_POLLING:
            spi_device_polling_transmit(spi_1, &t);
            break;
        case SPI_INTERRUPT:
            spi_device_transmit(spi_1, &t);
            break;
    }
}

void SpiReadWrite(spi_dev_t device, uint8_t * tx_buffer, uint8_t * rx_buffer, uint32_t buffer_size){
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length = buffer_size * 8;     // tx_buffer_size is in bytes, transaction length is in bits.
    t.rxlength = buffer_size * 8;
    t.tx_buffer = tx_buffer;        // Data
    t.rx_buffer = rx_buffer;
    switch(transfer_mode_1){
        case SPI_POLLING:
            spi_device_polling_transmit(spi_1, &t);
            break;
        case SPI_INTERRUPT:
            spi_device_transmit(spi_1, &t);
            break;
    }
}

uint8_t SpiDeInit(spi_dev_t device){
    return 0;
}

/** @} doxygen end group definition */
/*==================[end of file]============================================*/
