#pragma once
#include "core/disk.h"
#include "core/pci.h"

#include "util/math.h"
#include "types.h"

extern pci_driver_t ahci_driver;

/*

 * AHCI port control register structure
 * used as a part of the HBA memory structure (pointed by ABAR)

*/
typedef volatile struct {
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
} __attribute__((packed)) ahci_port_t;

/*

 * AHCI ABAR memory layout for the HBA
 * contains generic host control and port control registers

*/
typedef volatile struct {
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
  ahci_port_t ports[32]; // sizeof(ahci_mem->pi) * 8
} __attribute__((packed)) ahci_mem_t;

/*

 * command headers are used to create the command list, which is pointed
 * by ahci_port.clb each header in the command list is called a "slot"

*/
struct ahci_cmd_header {
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
};

/*

 * physical region descriptor, used by ahci_cmd_table to store multiple
 * PRDs these PRDs are stored in a PRD table (PRDT), and the table length
 * is specified by ahci_cmd_header.prdtl

*/
struct ahci_prd {
  // dword 0-1
  uint64_t dba;

  // dword 2
  uint32_t reserved0;

  // dword 3
  uint32_t dbc : 22;
#define AHCI_PRD_DATA_MAX (4 * 1024 * 1024)
  uint32_t reserved1 : 9;
  uint32_t interrupt : 1;
};

/*

 * command table structure, pointed by the ahci_cmd_header.ctba it's
 * used to store the the actual command

*/
struct ahci_cmd_table {
  uint8_t cfis[64];
  uint8_t acmd[16];
  uint8_t reserved[48];

  struct ahci_prd prdt[AHCI_PRDTL_MAX];
};

// port types and signatures
enum ahci_port_type {
  AHCI_PORT_TYPE_SATA = 0,
#define AHCI_SIGNATURE_SATA 0x101
  AHCI_PORT_TYPE_ATAPI = 1,
#define AHCI_SIGNATURE_ATAPI 0xEB140101
};

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

// Register host to device (H2D) FIS
struct sata_fis_h2d {
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
};

// Register device to host (D2H) FIS
struct sata_fis_d2h {
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
};

// PIO setup FIS
struct sata_fis_pio_setup {
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
};

// Data FIS
struct sata_fis_data {
  // dword 0
  uint8_t type;

  uint8_t pm_port   : 4;
  uint8_t reserved0 : 4;

  uint16_t reserved1;

  // dword 1...n
  uint32_t data[1];
};

// AHCI driver structures & functions
#define ahci_debg(f, ...) pdebg("AHCI: " f, ##__VA_ARGS__)
#define ahci_info(f, ...) pinfo("AHCI: " f, ##__VA_ARGS__)
#define ahci_fail(f, ...) pfail("AHCI: " f, ##__VA_ARGS__)
#define ahci_warn(f, ...) pwarn("AHCI: " f, ##__VA_ARGS__)

// different protocols our AHCI driver supports
typedef enum {
  AHCI_PROTOCOL_SATA,
  AHCI_PROTOCOL_ATAPI,
} ahci_protocol_t;

// stores information about a single available port
typedef struct {
  ahci_port_t *port; // port memory
  ahci_mem_t  *hba;  // HBA memory

  ahci_protocol_t protocol; // protocol used for port communication
  uint8_t         index;    // index of the port in the related HBA port list
  void           *vaddr;    // virtual base address (used calculate vaddrs of other structures)
  disk_t         *disk;     // a pointer to the disk object used for this device
} ahci_port_data_t;

// stores information about a single command
typedef struct {
  // input (used to setup the command)
  ahci_port_t *port;      // port memory
  void        *vaddr;     // virtual base address of the port
  uint64_t     fis_size;  // size of the command FIS
  uint64_t     data_size; // size of the data block (bytes)
  uint8_t     *data;      // vaddr pointer to the data block to read/write

  // output (obtained after calling ahci_cmd_setup() with the input)
  int8_t                  slot;   // command slot (number of the command header that is being used)
  struct ahci_cmd_header *header; // vaddr pointer to the command header
  struct ahci_cmd_table  *table;  // vaddr pointer to the command table
} ahci_cmd_t;

// general AHCI functions
int32_t ahci_init(pci_device_t *dev);
int32_t ahci_do(ahci_port_data_t *data, disk_op_t op, uint64_t lba, uint64_t sector_count, uint8_t *buf);

// port functions (core/ahci/port.c)
#define ahci_port_reset_is(port) (port->is = UINT32_MAX)
void *ahci_port_setup(ahci_port_t *port);
bool  ahci_port_stop(ahci_port_t *port);
bool  ahci_port_start(ahci_port_t *port);
bool  ahci_port_is_connected(ahci_port_t *port);
bool  ahci_port_is_busy(ahci_port_t *port);
bool  ahci_port_check_error(ahci_port_t *port, int64_t slot);

// command functions (core/ahci/cmd.c)
int32_t ahci_cmd_setup(ahci_cmd_t *cmd);
int32_t ahci_cmd_issue(ahci_cmd_t *cmd);

// SATA commands (core/ahci/sata.c)
int32_t ahci_sata_port_read(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf);
int32_t ahci_sata_port_write(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf);
int32_t ahci_sata_port_info(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf);

// ATAPI commands (core/ahci/atapi.c)
int32_t ahci_atapi_port_read(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf);
int32_t ahci_atapi_port_write(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf);
int32_t ahci_atapi_port_info(ahci_port_data_t *data, uint64_t lba, uint64_t sector_count, uint8_t *buf);
