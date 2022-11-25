#ifndef INC_ETX_OTA_UPDATE_MAIN_H_
#define INC_ETX_OTA_UPDATE_MAIN_H_

#define ETX_OTA_SOF  0xAA    // Start of Frame
#define ETX_OTA_EOF  0xBB    // End of Frame

#define ETX_APP_FLASH_ADDR 0x08040000   //Application's Flash Address

#define ETX_OTA_DATA_MAX_SIZE ( 1024 )  //Maximum data Size
#define ETX_OTA_DATA_OVERHEAD (    9 )  //data overhead
#define ETX_OTA_PACKET_MAX_SIZE ( ETX_OTA_DATA_MAX_SIZE + ETX_OTA_DATA_OVERHEAD )
#define ETX_OTA_MAX_FW_SIZE ( 1024 * 512 )


/*
 * Exception codes
 */
typedef enum
{
  ETX_OTA_EX_OK       = 0,    // Success
  ETX_OTA_EX_ERR      = 1,    // Failure
}ETX_OTA_EX_;

/*
 * OTA process state
 */
typedef enum
{
  ETX_OTA_STATE_IDLE    = 0,
  ETX_OTA_STATE_START   = 1,
  ETX_OTA_STATE_HEADER  = 2,
  ETX_OTA_STATE_DATA    = 3,
  ETX_OTA_STATE_END     = 4,
}ETX_OTA_STATE_;

/*
 * Packet type
 */
typedef enum
{
  ETX_OTA_PACKET_TYPE_CMD       = 0,    // Command
  ETX_OTA_PACKET_TYPE_DATA      = 1,    // Data
  ETX_OTA_PACKET_TYPE_HEADER    = 2,    // Header
  ETX_OTA_PACKET_TYPE_RESPONSE  = 3,    // Response
}ETX_OTA_PACKET_TYPE_;

/*
 * OTA Commands
 */
typedef enum
{
  ETX_OTA_CMD_START = 0,    // OTA Start command
  ETX_OTA_CMD_END   = 1,    // OTA End command
  ETX_OTA_CMD_ABORT = 2,    // OTA Abort command
}ETX_OTA_CMD_;

typedef struct
{
  uint8_t   sof;
  uint8_t   packet_type;
  uint8_t  data_len_1;
  uint8_t  data_len_2;
  uint8_t   cmd;
  uint32_t  crc;
  uint8_t   eof;
}__attribute__((packed)) ETX_OTA_COMMAND_;

typedef struct
{
  uint8_t     sof;
  uint8_t     packet_type;
  uint8_t    data_len_1;
  uint8_t    data_len_2;
  uint32_t    crc;
  uint8_t     eof;
}__attribute__((packed)) ETX_OTA_HEADER_;

typedef struct
{
  uint8_t     sof;
  uint8_t     packet_type;
  uint8_t    data_len_1;
  uint8_t    data_len_2;
  uint8_t     *data;
  uint8_t     eof;
}__attribute__((packed)) ETX_OTA_DATA_;

#endif /* INC_ETX_OTA_UPDATE_MAIN_H_ */