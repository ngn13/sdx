#pragma once
#include "core/pci.h"
#include "fs/disk.h"
#include "types.h"

extern pci_driver_t ahci_driver;

/*

 * AHCI port control register structure
 * used as a part of the HBA memory structure (pointed by ABAR)

*/
typedef volatile struct ahci_port {
  uint64_t clb; // command list base address
#define AHCI_PORT_CMD_LIST_COUNT  32
#define ahci_port_cmd_list_size() (sizeof(struct ahci_cmd_header) * AHCI_PORT_CMD_LIST_COUNT)
  uint64_t fb;  // (received) FIS base address
  uint32_t is;  // interrupt status
  uint32_t ie;  // interrupt enabled
  uint32_t cmd; // command and status
  uint32_t reserved0;
  uint32_t tfd;    // task file data
  uint32_t sig;    // signature
  uint32_t ssts;   // SATA status
  uint32_t sctl;   // SATA control
  uint32_t serr;   // SARA error
  uint32_t sact;   // SATA active
  uint32_t ci;     // command issue
  uint32_t sntf;   // SATA notification
  uint32_t fbs;    // FIS based switching
  uint32_t devslp; // device sleep

  uint32_t reserved[10];

  uint32_t vendor[4]; // vendor specific
} ahci_port_t;

/*

 * AHCI ABAR memory layout for the HBA
 * contains generic host control and port control registers

*/
typedef volatile struct ahci_mem {
  // generic host control (0x00 - 0x2C)
  uint32_t cap;     // host capabilities
  uint32_t ghc;     // global host control
  uint32_t is;      // interrupt status
  uint32_t pi;      // ports implemented
  uint32_t vs;      // version
  uint32_t ccc_ctl; // coalescing control
  uint32_t ccc_pts; // coalsecing ports (this is an actual typo in da spec lmao)
  uint32_t em_loc;  // enclosure managment location
  uint32_t em_ctl;  // enclosure managment control
  uint32_t cap2;    // capabilities extended
  uint32_t bohc;    // BIOS/OS handoff

  // reserved (0x2C - 0x60)
  uint8_t reserved[52];

  // reserved for NVMHCI (0x60 - 0xA0)
  uint8_t nvmhci[64];

  // vendor specific (0xA0 - 0x100)
  uint8_t vendor[96];

  // port control registers
  ahci_port_t ports[1];
} ahci_mem_t;

/*

 * command headers are used to create the command list, which is pointed by ahci_port.clb
 * each header in the command list is called a "slot"

*/
typedef struct ahci_cmd_header {
  // dword 0
  uint8_t cfl      : 5;
  uint8_t atapi    : 1;
  uint8_t write    : 1;
  uint8_t prefetch : 1;

  uint8_t reset     : 1;
  uint8_t bist      : 1;
  uint8_t clear     : 1;
  uint8_t reserved0 : 1;
  uint8_t pmp       : 4;

  uint16_t prdtl;
#define AHCI_PRDTL_MAX             8                                   // 8 PRDs per table
#define ahci_prdtl_from_size(size) (div_ceil(size, AHCI_PRD_DATA_MAX)) // calculate PRD count from sector count

  // dword 1
  volatile uint32_t prdbc;

  // dword 2-3
  uint64_t ctba;

  // dword 4-7
  uint32_t reserved1[4];
} ahci_cmd_header_t;

/*

 * physical region descriptor, used by ahci_cmd_table to store multiple PRDs
 * these PRDs are stored in a PRD table (PRDT), and the table length is specified by ahci_cmd_header.prdtl

*/
typedef struct ahci_prd {
  // dword 0-1
  uint64_t dba;

  // dword 2
  uint32_t reserved0;

  // dword 3
  uint32_t dbc : 22;
#define AHCI_PRD_DATA_MAX (4 * 1024 * 1024)
  uint32_t reserved1 : 9;
  uint32_t interrupt : 1;
} ahci_prd_t;

/*

 * command table structure, pointed by the ahci_cmd_header.ctba
 * it's used to store the the actual command

*/
typedef struct ahci_cmd_table {
  uint8_t cfis[64];
  uint8_t acmd[16];
  uint8_t reserved[48];

  ahci_prd_t prdt[AHCI_PRDTL_MAX];
} ahci_cmd_table_t;

// port types and signatures
typedef enum ahci_port_type {
  AHCI_PORT_TYPE_SATA = 0,
#define AHCI_SIGNATURE_SATA 0x101
  AHCI_PORT_TYPE_ATAPI = 1,
#define AHCI_SIGNATURE_ATAPI 0xEB140101
} ahci_port_type_t;

// ATA commands
enum ahci_ata_cmd {
  AHCI_ATA_PACKET = 0xa0, // used to transfer a SCSI command via the command packet (7.18 PACKET – A0h, Packet)
  AHCI_ATA_READ_DMA_EXT =
      0x25, // allows the host to read data using the DMA data transfer protocol (7.22 READ DMA EXT – 25h, DMA)
  AHCI_ATA_WRITE_DMA_EXT =
      0x35, // allows the host to write data using the DMA data transer protocol (7.59 WRITE DMA EXT – 35h, DMA)
  AHCI_ATA_IDENTIFY_DEVICE = 0xec, // provides information about the device (7.12 IDENTIFY DEVICE – ECh, PIO Data-In)
#define AHCI_ATA_IDENTIFY_DEVICE_DATA_SIZE                                                                             \
  512 // identify data requires 256 words (as shown in Table 45 — IDENTIFY DEVICE data)
};

typedef enum ahci_protocol {
  AHCI_PROTOCOL_SATA  = 0,
  AHCI_PROTOCOL_ATAPI = 1,
} ahci_protocol_t;

// stores information about available ports
typedef struct ahci_port_data {
  uint8_t         index;    // index of the port in the related HBA port list
  ahci_port_t    *port;     // port memory address
  ahci_mem_t     *base;     // HBA base address (HBA related with the port)
  ahci_protocol_t protocol; // protocol used for port communication
  disk_t         *disk;     // a pointer to the disk object used for this device
} ahci_port_data_t;

// stuff for ATAPI (see core/ahci/atapi.c)
bool ahci_atapi_port_read(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);
bool ahci_atapi_port_write(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);
bool ahci_atapi_port_info(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);

// stuff for SATA (see core/ahci/sata.c)
// Register host to device (H2D) FIS
typedef struct sata_fis_h2d {
  // dword 0
  uint8_t type;

  uint8_t pm_port   : 4;
  uint8_t reserved0 : 3;
  uint8_t c         : 1;
#define SATA_FIS_H2D_COMMAND 1
#define SATA_FIS_H2D_CONTROL 0

  uint8_t command;
  uint8_t feature_low;

  // dword 1
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;

  // dword 2
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t feature_high;

  // dword 3
  uint16_t count;
  uint8_t  icc;
  uint8_t  control;

  // dword 4
  uint16_t auxiliary;
  uint16_t reserved1;
} sata_fis_h2d_t;

// Register device to host (D2H) FIS
typedef struct sata_fis_d2h {
  // dword 0
  uint8_t type;

  uint8_t pm_port   : 4;
  uint8_t reserved0 : 2;
  uint8_t interrupt : 1;
  uint8_t reserved1 : 1;

  uint8_t status;
  uint8_t error;

  // dword 1
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;

  // dword 2
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t reserved2;

  // dword 3
  uint16_t count;
  uint16_t reserved3;

  // dword 4
  uint32_t reserved4;
} sata_fis_d2h_t;

// PIO setup FIS
typedef struct sata_fis_pio_setup {
  // dword 0
  uint8_t type;

  uint8_t pm_port   : 4;
  uint8_t reserved0 : 1;
  uint8_t direction : 1;
  uint8_t interrupt : 1;
  uint8_t reserved1 : 1;

  uint8_t status;
  uint8_t error;

  // dword 1
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;

  // dword 2
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t reserved2;

  // dword 3
  uint16_t count;
  uint8_t  reserved3;
  uint8_t  new_status;

  // dword 4
  uint16_t transfer_count;
  uint32_t reserved4;
} sata_fis_pio_setup_t;

// Data FIS
typedef struct sata_fis_data {
  // dword 0
  uint8_t type;

  uint8_t pm_port   : 4;
  uint8_t reserved0 : 4;

  uint16_t reserved1;

  // dword 1...n
  uint32_t data[1];
} sata_fis_data_t;

bool ahci_sata_port_read(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);
bool ahci_sata_port_write(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);
bool ahci_sata_port_info(ahci_port_data_t *data, uint64_t offset, uint64_t sector_count, uint8_t *buf);

// helper port commands (see core/ahci/port.c)
bool                   ahci_port_stop(ahci_port_t *port);
bool                   ahci_port_start(ahci_port_t *port);
bool                   ahci_port_check(ahci_port_t *port);
int8_t                 ahci_port_find_slot(ahci_port_t *port);
bool                   ahci_port_init(ahci_port_t *port);
bool                   ahci_port_check_tfd(ahci_port_t *port, int64_t slot);
bool                   ahci_port_issue_cmd(ahci_port_t *port, int8_t slot);
struct ahci_cmd_table *ahci_port_setup_header(
    ahci_cmd_header_t *header, uint64_t cmd_fis_size, bool write_to_device, uint64_t sector_count, uint8_t *buf);
#define ahci_port_get_slot(port, slot) (slot == -1 ? NULL : &((struct ahci_cmd_header *)port->clb)[slot])

// ahci driver entry
bool ahci_port_do(ahci_port_data_t *data, disk_op_t op, uint64_t offset, uint64_t sector_count, uint8_t *buf);
bool ahci_init(pci_device_t *dev);
