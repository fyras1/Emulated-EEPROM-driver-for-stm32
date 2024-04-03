
/*
 * eeprom_drv.h
 * fyras1
 *
 */

#ifndef EEPROM_EMUL_EEP_DRV_H_
#define EEPROM_EMUL_EEP_DRV_H_

#include "eeprom_mcu_itf.h"
#include "eeprom_drv_cfg.h"

#define NB_EEPROM_PAGES               ( 2U )
#define PAGE_0                        ( 0U )     /*DO NOT change page ID*/
#define PAGE_1                        ( 1U )     /*DO NOT change page ID*/
#define MAX_PAGE_ID                   ( NB_EEPROM_PAGES - 1U )

#define MAX_EEPROM_VARIABLES          ( ( EEPROM_PAGE_SIZE - PAGE_HEADER_SIZE ) / ( PACKET_SIZE ) ) /*useful, but not used*/

#define FLASH_EEPROM_END_ADDR         ( FLASH_EEPROM_START_ADDR + ( EEPROM_PAGE_SIZE * ( NB_EEPROM_PAGES ) ) )
#define PAGE_STATUS_SIZE              ( 4U )
#define PAGE_ERASE_COUNT_SIZE         ( 4U )
#define PAGE_HEADER_SIZE              ( PAGE_STATUS_SIZE + PAGE_ERASE_COUNT_SIZE )



#define VIRTUAL_ADDRESS_SIZE          ( 2U )
#define CRC_SIZE                      ( 2U )
#define DATA_SIZE                     ( 4U )

#define PACKET_SIZE                   ( VIRTUAL_ADDRESS_SIZE + CRC_SIZE + DATA_SIZE )
#define EMPTY_PACKET                  ( 0xFFFFFFFFFFFFFFFFU )     /*empty packet = not used yet*/
#define FREED_PACKET                  ( 0x0000000000000000U )     /*freed packet = has been used and no longer needed*/
#define NO_EMPTY_WRITE_SPACE_FOUND    ( 0xFFFFFFFFU )

#define NEXT_PAGE( pageId )                    ( pageId ^ 1U )
#define PAGE_HEADER_ADDRESS( pageId )          ( FLASH_EEPROM_START_ADDR + ( pageId * EEPROM_PAGE_SIZE ) )
#define PAGE_END_ADDRESS( pageId )             ( PAGE_HEADER_ADDRESS( pageId ) + EEPROM_PAGE_SIZE )
#define PAGE_BODY_ADDRESS( pageId )            ( PAGE_HEADER_ADDRESS( pageId ) + PAGE_HEADER_SIZE )
#define IS_ADDRESS_IN_EEPROM( ADDRESS )        ( ( ADDRESS >= FLASH_EEPROM_START_ADDR ) && ( ADDRESS < FLASH_EEPROM_END_ADDR ) )
#define IS_VIRTUAL_ADDRESS_VALID( ADDRESS )    ( ( ADDRESS > 0 ) && ( ADDRESS < 0xFFFF ) ) /*0x0000 and 0xffff mark freed and empty flash locations*/

/******************EEPROM RETURN CODES**********************/
#define Du8EEPROM_eSUCCESS            ( 0U )

#define Du8EEPROM_eERROR               ( 1U )
#define Du8EEPROM_eBAD_PARAM          ( 2U )
#define Du8EEPROM_eREAD_ERROR         ( 3U )
#define Du8EEPROM_eWRITE_ERROR        ( 4U )
#define Du8EEPROM_eERASE_ERROR        ( 5U )
#define Du8EEPROM_eALIGNMENT_ERROR    ( 6U )
#define Du8EEPROM_eDATA_CORRUPTED     ( 7U )
/*********************************************************/

/********************typedefs*************************/
typedef enum
{
    EEPROM_PAGE_UNDEFINED,
    EEPROM_PAGE_ACTIVE,
    EEPROM_PAGE_RECEIVING,
    EEPROM_PAGE_ERASED,
} EEpromHeaderTypedef;

typedef struct
{
    uint16_t u16VirtAddr;
    uint16_t u16CRC;
    uint32_t u32DataVal;
} Tst_EppromPacket;

/*********************Prototypes******  ********************/

/*external APIs*/

/**
 * @brief Initialize the EEPROM by checking the page headers and setting the active page and next write address
 * @return Status code indicating the result of the initialization
 */
uint8_t u8EEPROM_eInit( void );

/**
 * @brief Format the EEPROM by erasing both pages and setting the active page
 * @return Status code indicating the result of the formatting operation
 */
uint8_t u8EEPROM_eFormat( void );


/**
 * @brief Write a variable to the EEPROM based on the virtual address
 * @param Fu16VirtAddr Virtual address of the variable to write
 * @param Fu32Data Data value to write
 * @return Status code indicating the result of the write operation
 */
uint8_t u8EEPROM_eWriteVar( uint16_t Fu16VirtAddr,
                            uint32_t Fu32Data );


/**
 * @brief Read a variable from the EEPROM based on the virtual address
 * @param Fu16VirtAddr Virtual address of the variable to read
 * @param Fpu32Value Pointer to store the read value
 * @return Status code indicating the result of the read operation
 */
uint8_t u8EEPROM_eReadVar( uint16_t Fu16VirtAddr,
                           uint32_t * Fpu32Value );


/**
 * @brief Check the data integrity of the EEPROM
 * @return Status code indicating the result of the data integrity check
 */
uint8_t u8EEPROM_eCheckDataIntegrity( void );


/**
 * @brief Read all variables from the EEPROM and store them in the provided array
 * @param Fu64arr Pointer to the array to store the read variables
 * @param u32MaArrSize Maximum size of the array
 * @param Fu32Size Pointer to a variable to store the actual size of the read variables
 * @return Status code indicating the result of the read operation
 */
uint8_t u8EEPROM_eReadAllVar( Tst_EppromPacket * Fu64arr,
                              uint32_t u32MaArrSize,
                              uint32_t * Fu32Size );

/**
 * @brief Check if the EEPROM is erased
 * @return TRUE if both EEPROM pages (PAGE_0 and PAGE_1) are erased, FALSE otherwise
 */
BOOL bEEPROM_eIsEepromErased( void );


/**
 * @brief Get the erase count of a specific EEPROM page
 * @param Fu8PageId ID of the EEPROM page
 * @param Fpu32EraseCount Pointer to store the erase count
 * @return Status code indicating the result of the operation
 */
uint8_t u8EEPROM_iGetEraseCount( uint8_t Fu8PageId,
                                 uint32_t * Fpu32EraseCount );


#endif /* EEPROM_EMUL_EEP_DRV_H_ */
