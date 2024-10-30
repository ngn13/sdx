#pragma once
#include "types.h"

/*

 * frame information structure (FIS) types

 * FIS are simple data structures (like packets) that are used to communicate
 * with the device, the device also uses FIS to communicate with the host

*/

#define SATA_SECTOR_SIZE 512
#define SATA_SIGNATURE   0x101

typedef enum sata_fis_type {
  SATA_FIS_REG_H2D   = 0x27, // Register FIS - host to device
  SATA_FIS_REG_D2H   = 0x34, // Register FIS - device to host
  SATA_FIS_DMA_ACT   = 0x39, // DMA activate FIS - device to host
  SATA_FIS_DMA_SETUP = 0x41, // DMA setup FIS - bidirectional
  SATA_FIS_DATA      = 0x46, // Data FIS - bidirectional
  SATA_FIS_BIST      = 0x58, // BIST activate FIS - bidirectional
  SATA_FIS_PIO_SETUP = 0x5F, // PIO setup FIS - device to host
  SATA_FIS_DEV_BITS  = 0xA1, // Set device bits FIS - device to host
} sata_fis_type_t;
