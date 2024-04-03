/*
 * chnage HAL APIs with the equivalent of your project
 */
#include "eeprom_mcu_itf.h"
#include "eeprom_drv.h"

#if IS_FREERTOS_USED
#include "FreeRTOS.h"
#include "task.h"
#endif
/**
 * @brief Erase a sector of the MCU flash memory
 * @param Page Page number of the sector to erase
 * @return Status code indicating the result of the erase operation
 */
__attribute__((weak)) uint8_t u8FLASH_ITF_eFlashSectorErase( uint8_t Fu8Page )
{
    uint8_t ret = 0U;
    uint32_t u32SectorError = 0U;
    FLASH_EraseInitTypeDef eraseInit;

    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    eraseInit.Sector = MCU_PAGE_0_FLASH_SECTOR + Fu8Page;
    eraseInit.NbSectors = 1;

    /*NOTE: ErasePage automatically sets page state to ERASED(0xffffffff) (fismail)*/
    HAL_FLASH_Unlock();


    /* Critical Section */
#if IS_FREERTOS_USED
    taskENTER_CRITICAL();
#endif

    if( HAL_FLASHEx_Erase( &eraseInit, &u32SectorError ) != HAL_OK )
    {
        ret = 1U;
    }

    /*Exit Critical Section */
#if IS_FREERTOS_USED
    taskEXIT_CRITICAL();
#endif
    HAL_FLASH_Lock();
    return ret;
}



/**
 * @brief Program data into the MCU flash memory at the specified address
 * @param Fu32Address Address in the flash memory to write the data
 * @param Fu64Data Data to be written
 * @param fu8WriteSizeBytes Size of the data to be written in bytes
 * @return Status code indicating the result of the program operation : 0 OK ; 1 NOT OK
 */
__attribute__((weak)) uint8_t u8FLASH_ITF_FlashProgram( uint32_t Fu32Address,
                                  uint64_t Fu64Data,
                                  uint8_t fu8WriteSizeBytes )
{
    uint8_t u8FnRet = 0U;
    HAL_StatusTypeDef hal_status;

    HAL_FLASH_Unlock();

    switch( fu8WriteSizeBytes )
    {
        case 1:
           {
               if( HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE, Fu32Address, Fu64Data ) != HAL_OK )
               {
                   u8FnRet = 1U;
               }
           }
           break;

        case 2:
           {
               if( HAL_FLASH_Program( FLASH_TYPEPROGRAM_HALFWORD, Fu32Address, Fu64Data ) != HAL_OK )
               {
                   u8FnRet = 1U;
               }
           }
           break;

        case 4:
           {
               if( HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, Fu32Address, Fu64Data ) != HAL_OK )
               {
                   u8FnRet = 1U;
               }
           }
           break;

        case 8:
           {
#if IS_FREERTOS_USED
               taskENTER_CRITICAL(); /*double word write should NOT be interrupted*/
#endif
               hal_status = HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, Fu32Address, ( Fu64Data & 0xFFFFFFFFU ) );
               hal_status |= HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, Fu32Address + 4U, ( Fu64Data >> 32 ) & 0xFFFFFFFFU );
#if IS_FREERTOS_USED
               taskEXIT_CRITICAL();
#endif
               if( hal_status != HAL_OK )
               {
                   u8FnRet = 1U;
               }
           }
           break;

        default:
           {
               u8FnRet = Du8EEPROM_eERROR;
           }
           break;
    }

    HAL_FLASH_Lock();

    return u8FnRet;
}

/**
 * @}
 */
