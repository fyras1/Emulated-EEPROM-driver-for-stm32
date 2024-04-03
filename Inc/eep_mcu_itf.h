
#ifndef EEP_MCU_ITF_H_
#define EEP_MCU_ITF_H_


#include "stdint.h"
#include "stm32f2xx_hal.h"

#define MCU_PAGE_0_FLASH_SECTOR    (FLASH_SECTOR_2) /*FLASh_SECTOR_2 for stm32f2*/
#define MCU_PAGE_1_FLASH_SECTOR    (FLASH_SECTOR_3) /*FLASh_SECTOR_3 for stm32f2*/


/**
 * @brief Erase a sector of the MCU flash memory
 * @param Page Page number of the sector to erase
 * @return Status code indicating the result of the erase operation
 */
uint8_t u8FLASH_ITF_eFlashSectorErase( uint8_t Page );


/**
 * @brief Program data into the MCU flash memory at the specified address
 * @param Fu32Address Address in the flash memory to write the data
 * @param Fu64Data Data to be written
 * @param fu8WriteSizeBytes Size of the data to be written in bytes
 * @return Status code indicating the result of the program operation : 0 OK ; 1 NOT OK
 */
uint8_t u8FLASH_ITF_FlashProgram( uint32_t Fu32Address,
                                  uint64_t Fu64Data,
                                  uint8_t fu8WriteSizeBytes );


#endif /*EEP_MCU_ITF_H_*/
