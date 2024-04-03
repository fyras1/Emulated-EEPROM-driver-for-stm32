/*
 * eeprom_drv.c
 * fyras1
 *
 */

#include "eeprom_mcu_itf.h"
#include "eeprom_drv.h"




static uint8_t u8ActivePage = 0xFF;
static uint32_t u32NextWriteAddress = NO_EMPTY_WRITE_SPACE_FOUND;
#if EEPROM_DEBUG_MODE
    static uint32_t u32EraseCounter = 0U;
#endif
static BOOL bEEPROM_iInitDone = FALSE;



/*Internal -----------*/


/** @defgroup EEPROMPrivate_Func Private_Functions
 * @{
 */

static uint8_t u8EEPROM_iGetPageStatus( uint8_t Fu8PageId,
                                        uint32_t * Fpu32RetStatus );
static uint8_t u8EEPROM_iSetPageStatus( uint8_t Fu8PageId,
                                        uint32_t Fu32NewPageStatus );
static uint8_t u8EEPROM_iErasePage( uint8_t Fu8Page );
static uint8_t u8EEPROM_iWrite( uint32_t Fu32Address,
                                uint64_t Fu64Data,
                                uint8_t fu8WriteSizeBytes );
static uint8_t u8EEPROM_freeVar( uint64_t Fu16VirtAddr,
                                 uint32_t Fu32StartSearchAddr );
static uint16_t u16EEPROM_iCalculateCRC( uint16_t Fu16VirtAddr,
                                         uint32_t Fu32Data );
static uint32_t u32EEPROM_iRead( uint32_t Fu32Address );
static BOOL bEEPROM_isPageErased( const uint8_t Fu8PageId );
static uint8_t u8EEPROM_iPageTransfer( uint8_t Fu8PageIdSource,
                                       uint8_t Fu8PageIdDestination );
static uint8_t u8EEPROM_iRestarPagetTransfer( uint8_t Fu8PageIdSource,
                                              uint8_t Fu8PageIdDestination );
static EEpromHeaderTypedef eEEPROM_GetHeader( uint8_t eeprom_Page );
static uint32_t u32EEPROM_iFindNextWriteAddress( uint8_t u8pageId,
                                                 uint32_t * Fpu32NextWriteAddress );
/**
 * @}
 */

#if INTEGRATION_TEST_MODE
    extern BOOL bPageTransferCheck;
    extern BOOL bSetGetEraseCountOK;
#endif

/**
 * @brief Format the EEPROM by erasing both pages and setting the active page
 * @return Status code indicating the result of the formatting operation
 */
uint8_t u8EEPROM_eFormat( void )
{
    uint8_t u8FnRet = Du8EEPROM_eSUCCESS;

    u8FnRet = u8EEPROM_iErasePage( PAGE_0 );
    u8FnRet |= u8EEPROM_iErasePage( PAGE_1 );

    u8FnRet |= u8EEPROM_iSetPageStatus( PAGE_0, PAGE_STATUS_ACTIVE );
    u8FnRet |= u8EEPROM_iSetPageStatus( PAGE_1, PAGE_STATUS_ERASED );

    u8ActivePage = PAGE_0;
    u32NextWriteAddress = PAGE_HEADER_ADDRESS( PAGE_0 ) + PAGE_HEADER_SIZE;

    if( u8FnRet != Du8EEPROM_eSUCCESS )
    {
        return Du8EEPROM_eERROR;
    }

    return Du8EEPROM_eSUCCESS;
}


/**
 * @brief Set the status of a page in the EEPROM
 * @param Fu8PageId: Page ID of the EEPROM page
 * @param Fu32NewPageStatus: New status to be set for the page
 * @return Status code indicating the result of the operation
 */
static uint8_t u8EEPROM_iSetPageStatus( uint8_t Fu8PageId,
                                        uint32_t Fu32NewPageStatus )
{
    if( ( Fu8PageId > MAX_PAGE_ID ) ||
        ( ( Fu32NewPageStatus != PAGE_STATUS_ACTIVE ) &&
          ( Fu32NewPageStatus != PAGE_STATUS_RECEIVING ) &&
          ( Fu32NewPageStatus != PAGE_STATUS_ERASED ) ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    uint32_t u32CurrentPageStatus;

    if( Du8EEPROM_eSUCCESS != u8EEPROM_iGetPageStatus( Fu8PageId, &u32CurrentPageStatus ) )
    {
        return Du8EEPROM_eERROR;
    }

    if( ( u32CurrentPageStatus & Fu32NewPageStatus ) == Fu32NewPageStatus ) /*check that transition is possible (1 -> 0 ) (firas)*/
    {
        return u8EEPROM_iWrite( PAGE_HEADER_ADDRESS( Fu8PageId ), Fu32NewPageStatus, 4 );
    }
    else
    {
        return Du8EEPROM_eERROR;
    }
}

/**
 * @brief Get the status of a page in the EEPROM
 * @param Fu8PageId: Page ID of the EEPROM page
 * @param Fpu32RetStatus: Pointer to store the retrieved status
 * @return Status code indicating the result of the operation
 */
static uint8_t u8EEPROM_iGetPageStatus( uint8_t Fu8PageId,
                                        uint32_t * Fpu32RetStatus )
{
    uint32_t u32PageHeaderAddress;

    if( ( Fu8PageId > MAX_PAGE_ID ) || ( NULL == Fpu32RetStatus ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    u32PageHeaderAddress = PAGE_HEADER_ADDRESS( Fu8PageId );
    *Fpu32RetStatus = *( ( uint32_t * ) u32PageHeaderAddress );

    return Du8EEPROM_eSUCCESS;
}



/**
 * @brief Erase a page in the EEPROM
 * @param Fu8Page: Page ID of the EEPROM page to be erased
 * @return Du8EEPROM_eSUCCESS if the page is erased,Du8EEPROM_eBAD_PARAM if the page is erased, FALSE otherwise
 */
uint8_t u8EEPROM_iErasePage( uint8_t Fu8Page )
{
    uint32_t u32PageEraseCount = 0U;



    if( Fu8Page > MAX_PAGE_ID )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    /*NOTE: ErasePage automatically sets page state to ERASED(0xffffffff) (fismail)*/

    #if EEPROM_DEBUG_MODE
        u32EraseCounter++;
    #endif


    ( void ) u8EEPROM_iGetEraseCount( Fu8Page, &u32PageEraseCount );

    if( 0xFFFFFFFFU == u32PageEraseCount )
    {
        u32PageEraseCount = 0;
    }

    /*Erase Dedicated Sector*/
    if( u8FLASH_ITF_eFlashSectorErase( Fu8Page ) != 0 )
    {
        return Du8EEPROM_eERASE_ERROR;
    }

    /*write erase count*/
    ( void ) u8EEPROM_iWrite( ( PAGE_HEADER_ADDRESS( Fu8Page ) + PAGE_STATUS_SIZE ), u32PageEraseCount + 1U, 4U );



    return Du8EEPROM_eSUCCESS;
}



/**
 * @brief Get the erase count of a specific EEPROM page
 * @param Fu8PageId ID of the EEPROM page
 * @param Fpu32EraseCount Pointer to store the erase count
 * @return Status code indicating the result of the operation
 */
uint8_t u8EEPROM_iGetEraseCount( uint8_t Fu8PageId,
                                 uint32_t * Fpu32EraseCount )
{
    if( Fpu32EraseCount == NULL )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    uint32_t u32PageEeraseCountAddress = PAGE_HEADER_ADDRESS( Fu8PageId ) + PAGE_STATUS_SIZE;

    *( Fpu32EraseCount ) = *( ( uint32_t * ) u32PageEeraseCountAddress );

    return Du8EEPROM_eSUCCESS;
}


/**
 * @brief Get the header of an EEPROM page
 * @param eeprom_Page: Page ID of the EEPROM page
 * @return Header information of the EEPROM page
 */
EEpromHeaderTypedef eEEPROM_GetHeader( uint8_t eeprom_Page )
{
    uint32_t u32HeaderX;

    u32HeaderX = u32EEPROM_iRead( PAGE_HEADER_ADDRESS( eeprom_Page ) );

    switch( u32HeaderX )
    {
        case PAGE_STATUS_ERASED:
           {
               return EEPROM_PAGE_ERASED;
           }

        case PAGE_STATUS_ACTIVE:
           {
               return EEPROM_PAGE_ACTIVE;
           }

        case PAGE_STATUS_RECEIVING:
           {
               return EEPROM_PAGE_RECEIVING;
           }

        default:
            return EEPROM_PAGE_UNDEFINED;
    }
}


/**
 * @brief Initialize the EEPROM by checking the page headers and setting the active page and next write address
 * @return Status code indicating the result of the initialization
 */
uint8_t u8EEPROM_eInit( void )
{
    EEpromHeaderTypedef u32HeaderX0, u32HeaderX1;

    u32HeaderX1 = eEEPROM_GetHeader( PAGE_1 );

    switch( u32HeaderX1 )
    {
        case EEPROM_PAGE_ERASED:
           {
               /*P0 Active in all cases --------*/
               ( void ) u8EEPROM_iSetPageStatus( PAGE_0, PAGE_STATUS_ACTIVE );

               ( void ) u32EEPROM_iFindNextWriteAddress( PAGE_0, &u32NextWriteAddress );

               if( u32NextWriteAddress == NO_EMPTY_WRITE_SPACE_FOUND )
               {
                   if( Du8EEPROM_eSUCCESS != u8EEPROM_iPageTransfer( PAGE_0, PAGE_1 ) )
                   {
                       u8ActivePage = PAGE_0;
                       return Du8EEPROM_eERROR;
                   }
                   else
                   {
                       u8ActivePage = PAGE_1;

                       ( void ) u32EEPROM_iFindNextWriteAddress( PAGE_1, &u32NextWriteAddress );

                       if( u32NextWriteAddress == NO_EMPTY_WRITE_SPACE_FOUND )
                       {
                           return Du8EEPROM_eERROR;
                       }
                   }
               }
               else
               {
                   u8ActivePage = PAGE_0;
               }

               break;
           }


        case EEPROM_PAGE_RECEIVING:
           {
               u32HeaderX0 = eEEPROM_GetHeader( PAGE_0 );

               switch( u32HeaderX0 )
               {
                   case EEPROM_PAGE_ERASED:
                      {
                          /*in case voltage drop duing transfer from page0 to page1 */
                          ( void ) u8EEPROM_iSetPageStatus( PAGE_1, PAGE_STATUS_ACTIVE );
                          u8ActivePage = PAGE_1;
                          ( void ) u32EEPROM_iFindNextWriteAddress( PAGE_1, &u32NextWriteAddress );

                          if( u32NextWriteAddress == NO_EMPTY_WRITE_SPACE_FOUND )
                          {
                              return Du8EEPROM_eERROR;
                          }

                          break;
                      }

                   case EEPROM_PAGE_RECEIVING:
                      {
                          /*invalid state*/
                          ( void ) u8EEPROM_eFormat();
                          break;
                      }

                   case EEPROM_PAGE_ACTIVE:
                      {
                          /*power loss during data transfer from PAGE_0 to PAGE_1 */
                          /*=> Erase PAGE_1 (receiving ) and do restart transfer*/
                          ( void ) u8EEPROM_iRestarPagetTransfer( PAGE_0, PAGE_1 );

                          break;
                      }

                   case EEPROM_PAGE_UNDEFINED:
                      {
                          ( void ) u8EEPROM_eFormat();
                          break;
                      }

                   default:
                      {
                          /*undefined*/
                          ( void ) u8EEPROM_eFormat();

                          break;
                      }
               }

               break;
           }

        /*eeporm page 1 is active*/
        case EEPROM_PAGE_ACTIVE:
           {
               u32HeaderX0 = eEEPROM_GetHeader( PAGE_0 );

               switch( u32HeaderX0 )
               {
                   case EEPROM_PAGE_ERASED:
                      {
                          ( void ) u32EEPROM_iFindNextWriteAddress( PAGE_1, &u32NextWriteAddress );

                          if( u32NextWriteAddress == NO_EMPTY_WRITE_SPACE_FOUND )
                          {
                              if( Du8EEPROM_eSUCCESS != u8EEPROM_iPageTransfer( PAGE_1, PAGE_0 ) )
                              {
                                  u8ActivePage = PAGE_1;
                                  return Du8EEPROM_eERROR;
                              }
                              else
                              {
                                  u8ActivePage = PAGE_0;
                                  ( void ) u32EEPROM_iFindNextWriteAddress( PAGE_0, &u32NextWriteAddress );

                                  if( u32NextWriteAddress == NO_EMPTY_WRITE_SPACE_FOUND )
                                  {
                                      return Du8EEPROM_eERROR;
                                  }
                              }
                          }
                          else
                          {
                              u8ActivePage = PAGE_1;
                          }

                          break;
                      }

                   case EEPROM_PAGE_RECEIVING:
                      { /*restart transfer from page 1 to page 0*/
                          if( Du8EEPROM_eSUCCESS != u8EEPROM_iRestarPagetTransfer( PAGE_1, PAGE_0 ) )
                          {
                              return Du8EEPROM_eERROR;
                          }

                          break;
                      }

                   case EEPROM_PAGE_ACTIVE:
                      {
                          /*invalid state*/
                          ( void ) u8EEPROM_eFormat();
                          break;
                      }

                   case EEPROM_PAGE_UNDEFINED:
                      {
                          /*undefined*/
                          ( void ) u8EEPROM_eFormat();
                          break;
                      }

                   default:
                      {
                          /*undefined*/
                          ( void ) u8EEPROM_eFormat();

                          break;
                      }
               }

               break;
           }


        case EEPROM_PAGE_UNDEFINED:
           {
               /*undefined*/
               ( void ) u8EEPROM_eFormat();
               break;
           }

        default:
           {
               /*undefined*/
               ( void ) u8EEPROM_eFormat();

               break;
           }
    }

    /*mark init as done, can't write or read vars if init is not done*/
    /*also with init done == true, we're sure that u8ActivePage is initialized (firas)*/



    bEEPROM_iInitDone = TRUE;

    /*check integrity and removed redundant vars in they exist*/
    ( void ) u8EEPROM_eCheckDataIntegrity();

    return Du8EEPROM_eSUCCESS;
}

/**
 * @brief Transfer data from one page to another in the EEPROM
 * @param Fu8PageIdSource: Page ID of the source EEPROM page
 * @param Fu8PageIdDestination: Page ID of the destination EEPROM page
 * @return Status code indicating the result of the operation
 */
uint8_t u8EEPROM_iPageTransfer( uint8_t Fu8PageIdSource,
                                uint8_t Fu8PageIdDestination )
{
    uint64_t * pu64Counter = ( uint64_t * ) ( PAGE_HEADER_ADDRESS( Fu8PageIdSource ) + PAGE_HEADER_SIZE );
    uint32_t u32pageBodyEndAddress;
    uint64_t u64TempPacket;
    uint8_t u8FnRet;

    u32pageBodyEndAddress = PAGE_END_ADDRESS( Fu8PageIdSource );

    if( ( Fu8PageIdSource > MAX_PAGE_ID ) || ( Fu8PageIdDestination > MAX_PAGE_ID ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    /*STEP 0 : prepre destination page (erase + mark receiving)*/

    if( FALSE == bEEPROM_isPageErased( Fu8PageIdDestination ) )
    {
        u8FnRet = u8EEPROM_iErasePage( Fu8PageIdDestination );

        if( u8FnRet != Du8EEPROM_eSUCCESS )
        {
            return Du8EEPROM_eERROR;
        }
    }

    u8FnRet = u8EEPROM_iSetPageStatus( Fu8PageIdDestination, PAGE_STATUS_RECEIVING );

    if( u8FnRet != Du8EEPROM_eSUCCESS )
    {
        return Du8EEPROM_eERROR;
    }

    /*set new nextWriteAddress*/
    /*TODO check the line below for reentrancy problems (fismail)*/
    u32NextWriteAddress = PAGE_HEADER_ADDRESS( Fu8PageIdDestination ) + PAGE_HEADER_SIZE;

    /*STEP 1 : copy valid data from Fu8PageIdSource to Fu8PageIdDestination*/
    while( pu64Counter < ( uint64_t * ) u32pageBodyEndAddress )
    {
        u64TempPacket = *( pu64Counter );

        if( ( u64TempPacket != FREED_PACKET ) && ( u64TempPacket != EMPTY_PACKET ) )
        {
            if( u32NextWriteAddress < PAGE_END_ADDRESS( Fu8PageIdDestination ) )
            {
                ( void ) u8EEPROM_iWrite( u32NextWriteAddress, u64TempPacket, PACKET_SIZE );
                u32NextWriteAddress += PACKET_SIZE;
            }
            else
            {
                /*should not get here unless there are no redundant variables in pageSrc*/
                /*and the pageSrc was fully used (2047 distinct variables !!!) (fismail)*/

                return Du8EEPROM_eWRITE_ERROR;
            }
        }

        pu64Counter++;
    }

    /*TODO (VERY IMPORTANT) check setPageStatus order in case of power loss (fismail)*/
    u8FnRet = u8EEPROM_iErasePage( Fu8PageIdSource ); /*erase + set to ERASED 0xfff*/

    if( u8FnRet != Du8EEPROM_eSUCCESS )
    {
        return Du8EEPROM_eERROR;
    }

    ( void ) u8EEPROM_iSetPageStatus( Fu8PageIdSource, PAGE_STATUS_ERASED ); /* line can be removed*/
    ( void ) u8EEPROM_iSetPageStatus( Fu8PageIdDestination, PAGE_STATUS_ACTIVE );


    u8ActivePage = Fu8PageIdDestination;

    #if INTEGRATION_TEST_MODE
        bPageTransferCheck = TRUE;
    #endif


    return Du8EEPROM_eSUCCESS;
}


/**
 * @brief Restart a page transfer in the EEPROM
 * @param Fu8PageIdSource: Page ID of the source EEPROM page
 * @param Fu8PageIdDestination: Page ID of the destination EEPROM page
 * @return Status code indicating the result of the operation
 */
uint8_t u8EEPROM_iRestarPagetTransfer( uint8_t Fu8PageIdSource,
                                       uint8_t Fu8PageIdDestination )
{
    uint8_t u8FnRet;

    if( ( Fu8PageIdSource > MAX_PAGE_ID ) || ( Fu8PageIdDestination > MAX_PAGE_ID ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    /*erase the "receiving" page*/
    u8FnRet = u8EEPROM_iErasePage( Fu8PageIdDestination );

    if( u8FnRet != Du8EEPROM_eSUCCESS )
    {
        return u8FnRet;
    }
    else
    {
        /*page erased, header automatically set to 0xFFFFFFFF*/
        return u8EEPROM_iPageTransfer( Fu8PageIdSource, Fu8PageIdDestination );
    }
}


/**
 * @brief Find the next write address in a page of the EEPROM
 * @param u8pageId: Page ID of the EEPROM page
 * @param Fpu32NextWriteAddress: Pointer to store the next write address
 * @return Status code indicating the result of the operation
 */
uint32_t u32EEPROM_iFindNextWriteAddress( uint8_t u8pageId,
                                          uint32_t * Fpu32NextWriteAddress )
{
    uint64_t * pu64Counter = ( ( uint64_t * ) PAGE_BODY_ADDRESS( u8pageId ) );
    uint32_t u32pageEndAddress = PAGE_END_ADDRESS( u8pageId );


    if( Fpu32NextWriteAddress == NULL )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    if( ( u8pageId > MAX_PAGE_ID ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    while( pu64Counter < ( uint64_t * ) u32pageEndAddress )
    {
        if( *( pu64Counter ) == EMPTY_PACKET )
        {
            ( *Fpu32NextWriteAddress ) = ( uint32_t ) pu64Counter; /*found first empty packet in page*/
            return Du8EEPROM_eSUCCESS;
        }

        pu64Counter++;
    }

    ( *Fpu32NextWriteAddress ) = NO_EMPTY_WRITE_SPACE_FOUND;
    return Du8EEPROM_eSUCCESS;
}

/**
 * @brief Check if the EEPROM is erased
 * @return TRUE if both EEPROM pages (PAGE_0 and PAGE_1) are erased, FALSE otherwise
 */
BOOL bEEPROM_eIsEepromErased( void )
{
    return( ( bEEPROM_isPageErased( PAGE_0 ) == TRUE ) && ( bEEPROM_isPageErased( PAGE_1 ) == TRUE ) );
}


/**
 * @brief Check if a page in the EEPROM is erased
 * @param Fu8PageId: Page ID of the EEPROM page to be checked
 * @return TRUE if the page is erased (all packets are empty), FALSE otherwise
 */
BOOL bEEPROM_isPageErased( const uint8_t Fu8PageId )
{
    if( Fu8PageId > MAX_PAGE_ID )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    uint32_t u32pageAdress = PAGE_HEADER_ADDRESS( Fu8PageId ) + PAGE_HEADER_SIZE;

    while( u32pageAdress <= ( PAGE_END_ADDRESS( Fu8PageId ) - PACKET_SIZE ) )
    {
        if( *( uint64_t * ) u32pageAdress != EMPTY_PACKET )
        {
            return FALSE;
        }

        u32pageAdress += PACKET_SIZE;
    }

    return TRUE;
}


/**
 * @brief Write a variable to the EEPROM based on the virtual address
 * @param Fu16VirtAddr Virtual address of the variable to write
 * @param Fu32Data Data value to write
 * @return Status code indicating the result of the write operation
 */
uint8_t u8EEPROM_eWriteVar( uint16_t Fu16VirtAddr,
                            uint32_t Fu32Data )
{
    uint64_t u64Packet;

    #if ( WRITE_CORRECTION_ENABLE )
        uint8_t u8WrtiteRetries = 0U;
        BOOL bWriteProblem = FALSE;
    #endif
    uint64_t u64PacketRead;
    uint16_t u16packetCRC;

    if( bEEPROM_iInitDone == FALSE )
    {
        return Du8EEPROM_eERROR;
    }

    /*forbidden adresses*/
    if( ( Fu16VirtAddr == 0U ) || ( Fu16VirtAddr == 0xFFFFU ) )
    {
        return Du8EEPROM_eWRITE_ERROR;
    }

    u16packetCRC = u16EEPROM_iCalculateCRC( Fu16VirtAddr, Fu32Data );

    u64Packet = ( ( uint64_t ) Fu16VirtAddr << 48 ) + ( ( uint64_t ) u16packetCRC << 32 ) + ( ( uint64_t ) Fu32Data );

    if( Du8EEPROM_eSUCCESS == u8EEPROM_iWrite( u32NextWriteAddress, u64Packet, PACKET_SIZE ) )
    {
        /*by enabling WRITE_CORRECTION_ENABLE in the cfg file, the code below will detect if a writeVar operation
         *  was not successful and will attempt to write it at the next empty 64* address.
         * this feature was not fully tested (firas)*/
        #if ( WRITE_CORRECTION_ENABLE ) /*partially tested*/
            u64PacketRead = *( ( uint64_t * ) u32NextWriteAddress );

            while( ( u64PacketRead != u64Packet ) && ( u32NextWriteAddress < PAGE_END_ADDRESS( u8ActivePage ) - PACKET_SIZE ) )
            {
                bWriteProblem = TRUE;
                /*write error at address u32NextWriteAddress*/
                /*flash cell wearing  (firas)*/
                /*TODO : detect write error and write at another adress*/
                ( void ) u8EEPROM_iWrite( u32NextWriteAddress, FREED_PACKET, PACKET_SIZE );
                u32NextWriteAddress += PACKET_SIZE;
                ( void ) u8EEPROM_iWrite( u32NextWriteAddress, u64Packet, PACKET_SIZE );
                u64PacketRead = *( ( uint64_t * ) u32NextWriteAddress );
                u8WrtiteRetries++;

                if( ( u64PacketRead == u64Packet ) )
                {
                    bWriteProblem = FALSE;
                }

                /*we try to write at next address*/
            }

            if( bWriteProblem )
            {
                return Du8EEPROM_eWRITE_ERROR;
            }
        #endif /* if ( WRITE_CORRECTION_ENABLE ) */
        /*free already written variable if it exists*/

        /*if power shut down here, it won't cause problems after next page transfer
         * because ransfer happens from top to buttom (fismail)*/

        ( void ) u8EEPROM_freeVar( Fu16VirtAddr, ( u32NextWriteAddress - PACKET_SIZE ) );

        u32NextWriteAddress += PACKET_SIZE;

        if( u32NextWriteAddress >= PAGE_END_ADDRESS( u8ActivePage ) )
        {
            ( void ) u8EEPROM_iPageTransfer( u8ActivePage, u8ActivePage ^ 1U );
        }

        return Du8EEPROM_eSUCCESS;
    }

    return Du8EEPROM_eWRITE_ERROR;
}

/*TODO: (maybe) create a var struct that contains the amount of variable writes to know whether to free or not (firas)*/

/**
 * @brief Free a variable in the EEPROM based on the virtual address and starting search address
 * @param Fu16VirtAddr Virtual address of the variable to free
 * @param Fu32StartSearchAddr Starting address to search for the variable
 * @return Status code indicating the result of the free operation
 */
uint8_t u8EEPROM_freeVar( uint64_t Fu16VirtAddr,
                          uint32_t Fu32StartSearchAddr )
{
    uint64_t * u64PageCounter = ( uint64_t * ) Fu32StartSearchAddr;
    uint32_t u32pageStartAddress = PAGE_HEADER_ADDRESS( u8ActivePage ) + PAGE_HEADER_SIZE;
    BOOL bIsVarFound = FALSE;
    uint8_t ret = Du8EEPROM_eSUCCESS;

    if( FALSE == IS_ADDRESS_IN_EEPROM( Fu32StartSearchAddr ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    if( FALSE == IS_VIRTUAL_ADDRESS_VALID( Fu16VirtAddr ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    while( ( u64PageCounter >= ( uint64_t * ) u32pageStartAddress ) && ( FALSE == bIsVarFound ) )
    {
        if( ( uint16_t ) ( *( u64PageCounter ) >> 48 ) == Fu16VirtAddr )
        {
            /*mark packet as freed (pull value to 0 )*/
            ret |= u8EEPROM_iWrite( ( uint32_t ) u64PageCounter, FREED_PACKET, PACKET_SIZE );

            /*comment line below to loop through all eeprom page to free a var => not optimal for simple write operations (firas)*/
            bIsVarFound = TRUE;
        }

        u64PageCounter--;
    }

    return ret;
}


/**
 * @brief Read a variable from the EEPROM based on the virtual address
 * @param Fu16VirtAddr Virtual address of the variable to read
 * @param Fpu32Value Pointer to store the read value
 * @return Status code indicating the result of the read operation
 */
uint8_t u8EEPROM_eReadVar( uint16_t Fu16VirtAddr,
                           uint32_t * Fpu32Value )
{
    uint64_t * u64PageCounter;
    uint32_t u32Data;
    uint16_t u16CRC, u16VirtAddr;
    uint64_t u64Packet;

    if( bEEPROM_iInitDone == FALSE )
    {
        return Du8EEPROM_eERROR;
    }

    uint32_t u32pageStartAdress = PAGE_HEADER_ADDRESS( u8ActivePage ) + PAGE_HEADER_SIZE;

    u64PageCounter = ( uint64_t * ) ( u32NextWriteAddress - PACKET_SIZE );

    while( u64PageCounter >= ( uint64_t * ) u32pageStartAdress )
    {
        u64Packet = *( u64PageCounter );
        u16VirtAddr = ( uint16_t ) ( u64Packet >> 48 );

        if( u16VirtAddr == Fu16VirtAddr ) /*addr found*/
        {
            u16CRC = ( uint16_t ) ( u64Packet >> 32 );
            u32Data = ( uint32_t ) ( u64Packet );

            if( u16CRC == u16EEPROM_iCalculateCRC( u16VirtAddr, u32Data ) ) /*is CRC correct*/
            {
                *Fpu32Value = u32Data;

                return Du8EEPROM_eSUCCESS;
            }
            else
            {
                /*corrupted data*/
                return Du8EEPROM_eDATA_CORRUPTED;
            }
        }

        u64PageCounter--;
    }

    /*Virt address not found*/
    return Du8EEPROM_eREAD_ERROR;
}


/**
 * @brief Calculate CRC for EEPROM data
 * @param Fu16VirtAddr: Virtual address in the EEPROM
 * @param Fu32Data: Data for which CRC needs to be calculated
 * @return Calculated CRC value
 */
uint16_t u16EEPROM_iCalculateCRC( uint16_t Fu16VirtAddr,
                                  uint32_t Fu32Data )
{
    /*proposed method to calculate CRC of u32 Data and u16 address :
     * decompose u32 data into 2 u16
     * sum with u16 address
     * CRC = u16Addr + u160Data + u161Data;
     */
    uint32_t u32Sum = 0;
    uint16_t u16CRC = 0;

    u32Sum = Fu16VirtAddr + ( Fu32Data & 0xFFFFU ) + ( ( Fu32Data >> 16 ) & 0xFFFFU );

    u16CRC = ( u32Sum & 0xFFFFU );

    return u16CRC;
}


/**
 * @brief Write data to the EEPROM
 * @param Fu32Address: Address in the EEPROM to write the data
 * @param Fu64Data: Data to be written (up to 64 bits)
 * @param fu8WriteSizeBytes: Size of the data to be written in bytes
 * @return Status code indicating the result of the operation
 */
uint8_t u8EEPROM_iWrite( uint32_t Fu32Address,
                         uint64_t Fu64Data,
                         uint8_t fu8WriteSizeBytes )
{
    uint8_t u8FnRet;
    uint8_t u8Ret = Du8EEPROM_eSUCCESS;

    if( FALSE == IS_ADDRESS_IN_EEPROM( Fu32Address ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    if( fu8WriteSizeBytes == 0U )
    {
        return Du8EEPROM_eBAD_PARAM; /*Size of the data null*/
    }

    if( ( Fu32Address % fu8WriteSizeBytes ) != 0U )
    {
        return Du8EEPROM_eBAD_PARAM; /*alignment error*/
    }

    u8FnRet = u8FLASH_ITF_FlashProgram( Fu32Address, Fu64Data, fu8WriteSizeBytes );

    if( u8FnRet != Du8EEPROM_eSUCCESS )
    {
        u8Ret = Du8EEPROM_eERROR;
    }

    return u8Ret;
}



/**
 * @brief Read data from the EEPROM
 * @param Fu32Address: Address in the EEPROM to read the data
 * @return Read data from the EEPROM
 */
static uint32_t u32EEPROM_iRead( uint32_t Fu32Address )
{
    uint32_t u32Data = 0;

    u32Data = *( ( uint32_t * ) Fu32Address );

    return u32Data;
}


/*checks flash integrity (crc is correct for all stored values) after init*/


uint8_t u8EEPROM_eCheckDataIntegrity( void )
{
    uint64_t * u64PageCounter = ( uint64_t * ) ( u32NextWriteAddress - PACKET_SIZE );
    uint64_t u64Packet;
    uint32_t u32pageStartAdress = PAGE_HEADER_ADDRESS( u8ActivePage ) + PAGE_HEADER_SIZE;
    uint32_t u32Data;
    uint16_t u16VirtAddr, u16CRC;

    uint8_t u8FnRet = Du8EEPROM_eSUCCESS;

    if( ( bEEPROM_iInitDone == FALSE ) || ( u8ActivePage == 0xFFU ) )
    {
        return Du8EEPROM_eERROR;
    }

    while( u64PageCounter >= ( uint64_t * ) u32pageStartAdress )
    {
        u64Packet = *( u64PageCounter );

        if( ( u64Packet != FREED_PACKET ) && ( u64Packet != EMPTY_PACKET ) )
        {
            u16VirtAddr = ( uint16_t ) ( u64Packet >> 48 );

            u16CRC = ( uint16_t ) ( u64Packet >> 32 );
            u32Data = ( uint32_t ) ( u64Packet );

            if( u16CRC != u16EEPROM_iCalculateCRC( u16VirtAddr, u32Data ) ) /*is CRC correct*/
            {
                u8FnRet = Du8EEPROM_eDATA_CORRUPTED;

                return u8FnRet;
            }

            /*the freeVar call was made to free old variables in case of power shut between write and free*/
            u8FnRet = u8EEPROM_freeVar( u16VirtAddr, ( ( uint32_t ) u64PageCounter - PACKET_SIZE ) );
        }

        u64PageCounter--;
    }

    if( u8FnRet != Du8EEPROM_eSUCCESS )
    {
        return Du8EEPROM_eERROR;
    }
    else
    {
        return Du8EEPROM_eSUCCESS;
    }
}


/*
 * loops through active page and reads all variables
 */

/**
 * @brief Read all variables from the EEPROM and store them in the provided array
 * @param Fu64arr Pointer to the array to store the read variables
 * @param u32MaArrSize Maximum size of the array
 * @param Fu32Size Pointer to a variable to store the actual size of the read variables
 * @return Status code indicating the result of the read operation
 */
uint8_t u8EEPROM_eReadAllVar( Tst_EppromPacket * Fu64arr,
                              uint32_t u32MaArrSize,
                              uint32_t * Fu32Size )
{
    uint32_t u32pageStartAdress;
    uint64_t * u64PageCounter;

    uint64_t u64Packet;

    uint32_t u32Data;

    uint16_t u16VirtAddr, u16CRC;


    if( ( bEEPROM_iInitDone == FALSE ) || ( u8ActivePage == 0xFFU ) )
    {
        return Du8EEPROM_eERROR;
    }

    if( ( Fu64arr == NULL ) || ( Fu32Size == NULL ) )
    {
        return Du8EEPROM_eBAD_PARAM;
    }

    *Fu32Size = 0;


    u32pageStartAdress = PAGE_HEADER_ADDRESS( u8ActivePage ) + PAGE_HEADER_SIZE;
    u64PageCounter = ( uint64_t * ) ( u32NextWriteAddress - PACKET_SIZE );

    while( u64PageCounter >= ( uint64_t * ) u32pageStartAdress )
    {
        u64Packet = *( u64PageCounter );

        if( u64Packet != FREED_PACKET )
        {
            u16VirtAddr = ( uint16_t ) ( u64Packet >> 48 );

            u16CRC = ( uint16_t ) ( u64Packet >> 32 );
            u32Data = ( uint32_t ) ( u64Packet );

            if( u16CRC != u16EEPROM_iCalculateCRC( u16VirtAddr, u32Data ) ) /*is CRC correct*/
            {
                /*return Du8EEPROM_eDATA_CORRUPTED;*/
            }
            else
            {
                Fu64arr[ ( *Fu32Size ) ].u16VirtAddr = u16VirtAddr;
                Fu64arr[ ( *Fu32Size ) ].u16CRC = u16CRC;
                Fu64arr[ ( *Fu32Size ) ].u32DataVal = u32Data;
                ( *Fu32Size )++;

                if( *Fu32Size > u32MaArrSize )
                {
                    return Du8EEPROM_eBAD_PARAM;
                }
            }
        }

        u64PageCounter--;
    }

    return Du8EEPROM_eSUCCESS;
}
