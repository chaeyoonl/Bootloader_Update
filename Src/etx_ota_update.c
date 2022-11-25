/*
 * etx_ota_update.c
 *
 *  Created on: 26-Jul-2021
 *      Author: EmbeTronicX
 */

#include <stdio.h>
#include "etx_ota_update.h"
#include "main.h"
#include <string.h>
#include <stdbool.h>
#include "rb.h"

/* Buffer to hold the received data */
static uint8_t Rx_Buffer[ ETX_OTA_PACKET_MAX_SIZE ];

/* OTA State */
static ETX_OTA_STATE_ ota_state = ETX_OTA_STATE_IDLE;

/* Firmware Total Size that we are going to receive */
static uint8_t ota_fw_total_size;
/* Firmware image's CRC32 */
static uint32_t ota_fw_crc;
/* Firmware Size that we have received */
static uint32_t ota_fw_received_size;

static uint16_t getDataLength(uint8_t *buf, int index);
static uint16_t etx_receive_chunk( uint8_t *buf, uint16_t max_len );
static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len );
static void etx_ota_send_resp( uint8_t type );
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                        uint16_t data_len, bool is_full_image );

static UART_HandleTypeDef huart1;
extern RingFifo_t gtUart1Fifo;

/**
  * @brief Download the application from UART and flash it.
  * @param None
  * @retval ETX_OTA_EX_
  */
ETX_OTA_EX_ etx_ota_download_and_flash()
{
  ETX_OTA_EX_ ret  = ETX_OTA_EX_OK;
  uint16_t    len;

  printf("Waiting for the OTA data...\r\n");

  /* Reset the variables */
  ota_fw_total_size    = 0u;
  ota_fw_received_size = 0u;
  ota_fw_crc           = 0u;
  ota_state            = ETX_OTA_STATE_START;

  do
  {
    //clear the buffer
    memset( Rx_Buffer, 0, ETX_OTA_PACKET_MAX_SIZE );

    len = etx_receive_chunk( Rx_Buffer, ETX_OTA_PACKET_MAX_SIZE );

    if( len != 0u )
    {
      ret = etx_process_data( Rx_Buffer, len );
    }
    else
    {
      //didn't received data. break.
      ret = ETX_OTA_EX_ERR;
    }

    //Send ACK or NACK
    if( ret != ETX_OTA_EX_OK )
    {
      printf("Sending NACK\r\n");
      break;
    }

  }while( ota_state != ETX_OTA_STATE_IDLE );

  return ret;
}

static uint16_t getDataLength(uint8_t *buf, int index) {
	uint16_t data_len = 0;

	if (buf[index-1] > 0x00) {	//length_2
	    	data_len = (buf[index-1] << 8) | buf[index];
	    }else {
	    	data_len = *(uint16_t *)&buf[index];
	    }

	return data_len;
}

/**
  * @brief Process the received data from UART4.
  * @param buf buffer to store the received data
  * @param max_len maximum length to receive
  * @retval ETX_OTA_EX_
  */
static ETX_OTA_EX_ etx_process_data( uint8_t *buf, uint16_t len )
{
  ETX_OTA_EX_ ret = ETX_OTA_EX_ERR;

  do
  {
    if( ( buf == NULL ) || ( len == 0u) )
    {
      break;
    }

    //Check we received OTA Abort command
    ETX_OTA_COMMAND_ *cmd = (ETX_OTA_COMMAND_*)buf;
    if( cmd->packet_type == ETX_OTA_PACKET_TYPE_CMD )
    {
      if( cmd->cmd == ETX_OTA_CMD_ABORT )
      {
        //received OTA Abort command. Stop the process
        break;
      }
    }

    switch( ota_state )
    {
      case ETX_OTA_STATE_IDLE:
      {
        printf("ETX_OTA_STATE_IDLE...\r\n");
        ret = ETX_OTA_EX_OK;
      }
      break;

      case ETX_OTA_STATE_START:
      {
        ETX_OTA_COMMAND_ *cmd = (ETX_OTA_COMMAND_*)buf;

        if( cmd->packet_type == ETX_OTA_PACKET_TYPE_CMD )
        {
          if( cmd->cmd == ETX_OTA_CMD_START )
          {
            printf("Received OTA START Command\r\n");
            ota_state = ETX_OTA_STATE_HEADER;
            ret = ETX_OTA_EX_OK;
          }
        }
      }
      break;

      case ETX_OTA_STATE_HEADER:
      {
        ETX_OTA_HEADER_ *header = (ETX_OTA_HEADER_*)buf;
        if( header->packet_type == ETX_OTA_PACKET_TYPE_HEADER )
        {
          ota_fw_total_size = buf[3];
          ota_state = ETX_OTA_STATE_DATA;
          ret = ETX_OTA_EX_OK;
        }
      }
      break;


      case ETX_OTA_STATE_DATA:
      {
        ETX_OTA_DATA_     *data     = (ETX_OTA_DATA_*)buf;
        uint16_t          data_len = getDataLength(buf, 3);
        HAL_StatusTypeDef ex;

        if( data->packet_type == ETX_OTA_PACKET_TYPE_DATA )
        {
          /* write the chunk to the Flash (App location) */
          ex = write_data_to_flash_app( buf, data_len, ( ota_fw_received_size == 0) );

          if( ex == HAL_OK )
          {
            if( ota_fw_total_size > 0 )
            {
            	ota_fw_total_size-=1;
            }else {
            	//received the full data. So, move to end
            	ota_state = ETX_OTA_STATE_END;
            }
            ret = ETX_OTA_EX_OK;
          }
        }
      }
      break;

      case ETX_OTA_STATE_END:
      {

        ETX_OTA_COMMAND_ *cmd = (ETX_OTA_COMMAND_*)buf;

        if( cmd->packet_type == ETX_OTA_PACKET_TYPE_CMD )
        {
          if( cmd->cmd == ETX_OTA_CMD_END )
          {
            printf("Received OTA END Command\r\n");

            //TODO: Very full package CRC

            ota_state = ETX_OTA_STATE_IDLE;
            ret = ETX_OTA_EX_OK;
          }
        }
      }
      break;

      default:
      {
        /* Should not come here */
        ret = ETX_OTA_EX_ERR;
      }
      break;
    };
  }while( false );

  return ret;
}

/**
  * @brief Receive a one chunk of data.
  * @param buf buffer to store the received data
  * @param max_len maximum length to receive
  * @retval ETX_OTA_EX_
  */
static uint16_t etx_receive_chunk( uint8_t *buf, uint16_t max_len )
{
  int16_t  ret;
  uint16_t data_len;
  int index = 0;

  do
  {
	buf[index++] = RB_read(&gtUart1Fifo);	//SOF

    if( buf[index-1] != ETX_OTA_SOF )
    {
      //Not received start of frame
      ret = ETX_OTA_EX_ERR;
      break;
    }

    buf[index++] = RB_read(&gtUart1Fifo);	//packet type

    buf[index++] = RB_read(&gtUart1Fifo);	//length_1
    buf[index++] = RB_read(&gtUart1Fifo);	//length_2


    data_len = getDataLength(buf, index-1);

    if (buf[1] != 2) {
    	for( uint16_t i = 0u; i < data_len; i++ )
    		{
    			buf[index++] = RB_read(&gtUart1Fifo);
    		}
    }

    index += 1u;
    buf[index++] = RB_read(&gtUart1Fifo);	//CRC

    //TODO: Add CRC verification

    do{
    	buf[index] = RB_read(&gtUart1Fifo);	//EOF
    }while(buf[index] != ETX_OTA_EOF);

  }while( false );

  if( max_len < index )
  {
    printf("Received more data than expected. Expected = %d, Received = %d\r\n",
                                                              max_len, index );
    index = 0u;
  }

  return index;
}

/**
  * @brief Write data to the Application's actual flash location.
  * @param data data to be written
  * @param data_len data length
  * @is_first_block true - if this is first block, false - not first block
  * @retval HAL_StatusTypeDef
  */
static HAL_StatusTypeDef write_data_to_flash_app( uint8_t *data,
                                        uint16_t data_len, bool is_first_block )
{
  HAL_StatusTypeDef ret;

  do
  {
    ret = HAL_FLASH_Unlock();
    if( ret != HAL_OK )
    {
      break;
    }

    //No need to erase every time. Erase only the first time.
    if( is_first_block )
    {

      printf("Erasing the Flash memory...\r\n");
      //Erase the Flash
      FLASH_EraseInitTypeDef EraseInitStruct;
      uint32_t SectorError;

      EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
      EraseInitStruct.Sector        = FLASH_SECTOR_5;
      EraseInitStruct.NbSectors     = 2;                    //erase 2 sectors(5,6)
      EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;

      ret = HAL_FLASHEx_Erase( &EraseInitStruct, &SectorError );
      if( ret != HAL_OK )
      {
        break;
      }
    }

    for(int i = 0; i < data_len; i++ )
    {
      ret = HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE,
                               (ETX_APP_FLASH_ADDR + ota_fw_received_size),
                               data[4+i]
                             );
      if( ret == HAL_OK )
      {
        //update the data count
        ota_fw_received_size += 1;
      }
      else
      {
        printf("Flash Write Error\r\n");
        break;
      }
    }

    if( ret != HAL_OK )
    {
      break;
    }

    ret = HAL_FLASH_Lock();
    if( ret != HAL_OK )
    {
      break;
    }
  }while( false );

  return ret;
}
