/*
 * eep_drv_cfg.h
 *
 *  Created on: 12 mai 2023
 *      Author: fismail
 */

#ifndef EEPROM_EMUL_INC_EEP_DRV_CFG_H_
#define EEPROM_EMUL_INC_EEP_DRV_CFG_H_



#define IS_FREERTOS_USED				( 0U )

/*map to two flash blocks with identical size*/
#define PAGE_0_FLASH_SECTOR            ( 0x08008000U ) /*FLASh_SECTOR_2 for stm32f205*/
#define PAGE_1_FLASH_SECTOR            ( 0x0800C000U ) /*FLASh_SECTOR_3 for stm32f205*/
#define EEPROM_PAGE_SIZE               ( 16U * 1024U )   /*for a 16KB flash block (stm32f205)*/


#define FLASH_EEPROM_START_ADDR    ( PAGE_0_FLASH_SECTOR ) /*map to your desired eeprom start block*/

/*if you want to change STATUS values, make sure the new values can go from state
 * ERASED -> RECEIVING -> ACTIVE by only writing zeros (0) to their binary representations
 *  RECOMMENDED: don't change it.*/
#define PAGE_STATUS_ERASED         ( 0xFFFFFFFF )
#define PAGE_STATUS_RECEIVING      ( 0xAAAAAAAA )
#define PAGE_STATUS_ACTIVE         ( 0x00000000 )

/* reads value after each write and verifies that it write wasn't corrupted, it will try to write it in another adress*/
#define WRITE_CORRECTION_ENABLE    ( 1U )


typedef uint8_t BOOL;

#define TRUE		(1U)
#define FALSE		(0U)

#endif /* EEPROM_EMUL_INC_EEP_DRV_CFG_H_ */
