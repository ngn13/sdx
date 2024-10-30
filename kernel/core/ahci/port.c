#include "core/ahci.h"
#include "fs/disk.h"
#include "mm/pm.h"

#include "util/bit.h"
#include "util/math.h"
#include "util/mem.h"
#include "util/printk.h"

enum ahci_port_cmd_bits {
  AHCI_PxCMD_ST  = 0, // start
  AHCI_PxCMD_CLO = 3, // command list overwrite
  // ...
  AHCI_PxCMD_FRE = 4, // FIS receive enable
  // ...
  AHCI_PxCMD_FR = 14, // FIS receive running
  AHCI_PxCMD_CR = 15, // command list running
  // ...
};

enum ahci_port_tfd_bits {
  AHCI_PxTFD_STS_ERR = 0, // (status) error
  // ...
  AHCI_PxTFD_STS_BSY = 7, // (status) busy
  // ...
  AHCI_PxTFD_STS_DRQ = 3, // (status) data transfer request
  // ...
  AHCI_PxTFD_ERR = 8 // error
};

// 3.3.10 Offset 28h: PxSSTS – Port x Serial ATA Status (SCR0: SStatus)
enum ahci_port_ssts_values {
  // ...
  AHCI_PxSSTS_IPM_ACTIVE = 1,
  AHCI_PxSSTS_DET_OK     = 3,
  // ...
};

// each ahci_port.fb points to a received FIS
struct ahci_received_fis {
  // DMA setup FIS
  uint8_t dsfis[28];
  uint8_t padding0[4];

  // PIO setup FIS
  sata_fis_pio_setup_t psfis;
  uint8_t              padding1[12];

  // D2H register FIS
  sata_fis_d2h_t rfis;
  uint8_t        padding2[4];

  // set device bits FIS
  uint8_t sdbfis[8];

  // unknown FIS
  uint8_t ufis[64];

  uint8_t reserved[96];
};

#define ahci_port_is_busy(port)                                                                                        \
  (bit_get(port->tfd, AHCI_PxTFD_STS_BSY) == 1 || bit_get(port->tfd, AHCI_PxTFD_STS_DRQ) == 1)

/*

 * stops the HBA from pushing commands to the device

 * used during the configuration of the device to prevent
 * writes/reads from partially written/configured memory

*/
bool ahci_port_stop(ahci_port_t *port) {
  bit_set(port->cmd, AHCI_PxCMD_ST, 0);

  while (bit_get(port->cmd, AHCI_PxCMD_CR) != 0)
    continue;

  bit_set(port->cmd, AHCI_PxCMD_FRE, 0);

  while (bit_get(port->cmd, AHCI_PxCMD_FR) != 0)
    continue;

  return true;
}

/*

 * starts the stoppen HBA back again
 * used after when the configuration of the device is completed

*/
bool ahci_port_start(ahci_port_t *port) {
  while (bit_get(port->cmd, AHCI_PxCMD_CR) == 1)
    continue;

  bit_set(port->cmd, AHCI_PxCMD_ST, 1);
  bit_set(port->cmd, AHCI_PxCMD_FRE, 1);

  return true;
}

// reset the port as described in 10.4.2 ("Port Reset")
bool ahci_port_reset(ahci_port_t *port) {
  if (!ahci_port_stop(port))
    return false;

  // COMRESET
  port->ssts &= ~0b111; // clear DEt

  while ((port->ssts & 0x0F) != AHCI_PxSSTS_DET_OK)
    continue;

  port->serr = UINT32_MAX; // clear error status

  if (!ahci_port_start(port))
    return false;

  return true;
}

/*

 * checks the DET, IPM and the signature to the port is connected to a supported device

 * on the specified port, if there's, then the function returns true
 * if not, then returns false

*/
bool ahci_port_check(ahci_port_t *port) {
  uint8_t det = port->ssts & 0x0F;       // device detection (bits 0-3)
  uint8_t ipm = (port->ssts >> 8) & 0xF; // interface power managment (bits 8-11)

  if (det != AHCI_PxSSTS_DET_OK || ipm != AHCI_PxSSTS_IPM_ACTIVE)
    return false;

  /*

   * reset da port bc signature is not correct for some fucking reason
   * maybe the first D2H is not received yet or something idfk

  */
  if (!ahci_port_reset(port)) {
    printk(KERN_DEBG, "AHCI: (port 0x%x) failed to reset during check\n", port);
    return false;
  }

  return port->sig == AHCI_SIGNATURE_SATA || port->sig == AHCI_SIGNATURE_ATAPI;
}

// finds an available command slot (command header)
int8_t ahci_port_find_slot(ahci_port_t *port) {
  for (uint8_t i = 0; i < AHCI_PORT_CMD_LIST_COUNT; i++) {
    if (bit_get(port->sact, i) == 0 && bit_get(port->ci, i) == 0)
      return i;
  }
  return -1;
}

bool ahci_port_init(ahci_port_t *port) {
  // stop sending SATA commands to the device
  if (!ahci_port_stop(port)) {
    printk(KERN_DEBG, "AHCI: (port 0x%x) failed to stop during initialization\n", port);
    return false;
  }

  // calculate the size required for the command list and the 256 byte aligned received FIS
  uint64_t command_table_offset[AHCI_PORT_CMD_LIST_COUNT] = {0};
  uint64_t received_fis_offset                            = 0;
  uint8_t  i                                              = 0;

  // command list size (command header size * command header count)
  uint64_t size       = ahci_port_cmd_list_size();
  uint64_t page_count = 0;

  // offset to make sure the received FIS is 256 byte aligned
  while (size % 256 != 0)
    size++;
  received_fis_offset = size;

  // received FIS size
  size += sizeof(struct ahci_received_fis);

  // command table size * command header count
  for (i = 0; i < AHCI_PORT_CMD_LIST_COUNT; i++) {
    // offset to make sure the command table is 128 byte aligned
    while (size % 128 != 0)
      size++;
    command_table_offset[i] = size;
    size += sizeof(struct ahci_cmd_table);
  }

  page_count = div_ceil(size, PM_PAGE_SIZE);

  /*

   * each port has a CLB (Command List Base Address) which points to a command list
   * and the command list is basically a list of 32 ahci_cmd_headers

   * the intereseting thing about the command list is that it needs to be a 1024 byte aligned address
   * as the lower 10 bits of the address is reserved for some fucking reason

  */
  port->clb = (uint64_t)pm_alloc(page_count);
  pm_set(port->clb, page_count, PM_ENTRY_FLAGS_DEFAULT | PM_ENTRY_FLAG_PCD);
  bzero((void *)port->clb, size);

  /*

   * each port also has a FB (FIS Base Address) which points to a Received FIS object
   * and this address needs to be 256 byte aligned address
   * as the lower 9 bits of the address is reserved (again for some random fucking reason)

  */
  port->fb = port->clb + received_fis_offset;

  /*

   * each ahci_cmd_header.ctba points to a command table, and command table addresses
   * are 128 byte aligned (as the lower 7 bits are reserved for another random fucking reason)

  */
  struct ahci_cmd_header *header = (void *)port->clb;

  for (i = 0; i < AHCI_PORT_CMD_LIST_COUNT; i++) {
    header[i].prdtl = AHCI_PRDTL_MAX;
    header[i].ctba  = port->clb + command_table_offset[i];
  }

  // clear interrupt status and enable all interrupts
  port->is = UINT32_MAX;
  port->ie = 1;

  if (!ahci_port_start(port)) {
    printk(KERN_DEBG, "AHCI: (port 0x%x) failed to start during initialization\n", port);
    return false;
  }

  return true;
}

/*

 * sets command header and related table with the given sector count and the buffer
 * and it returns the table used for data blocks

*/
struct ahci_cmd_table *ahci_port_setup_header(
    struct ahci_cmd_header *header, uint64_t cmd_fis_size, bool write_to_device, uint64_t size, uint8_t *buf) {
  // check the header (make sure it points to valid memory)
  if (NULL == header)
    return NULL;

  // check the FIS size (FISes are parted into different dwords)
  if (cmd_fis_size % sizeof(uint32_t) != 0)
    return NULL;

  struct ahci_cmd_table *table = NULL;
  uint16_t               i     = 0;

  /*

   * now we can setup the header:
   * - cfl: size of the command (in dwords)
   * - write: 1 = write from host to device, 0 = write from device to host
   * - prdtl: PRD count calculated from the given sector count

  */
  header->cfl   = cmd_fis_size / sizeof(uint32_t);
  header->write = write_to_device ? 1 : 0;
  header->prdtl = ahci_prdtl_from_size(size);

  // obtain and clear the command table
  table = (void *)header->ctba;
  bzero(table, sizeof(struct ahci_cmd_table));

  // setup all the PRDs
  for (; i < header->prdtl; i++) {
    table->prdt[i].dba = (uint64_t)buf; // set the data block base address
    // set the data block size (1 means 2, so we subtract one)
    if (i == header->prdtl - 1)
      // last PRD's data block size is calculated from left over sector count
      table->prdt[i].dbc = size - 1;
    else
      // other PRD's just use the data block with the max size
      table->prdt[i].dbc = AHCI_PRD_DATA_MAX - 1;
    table->prdt[i].interrupt = 0; // don't send an interrupt when the data block transfer is completed

    buf += AHCI_PRD_DATA_MAX;  // the next buffer address
    size -= AHCI_PRD_DATA_MAX; // left over size
  }

  return table;
}

// check if TFD register contains an error, if so log the error
bool ahci_port_check_tfd(ahci_port_t *port, int64_t slot) {
  if (bit_get(port->tfd, AHCI_PxTFD_STS_ERR) == 1) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) transfer error (TFD_STS_ERR)\n", port, slot);
    return false;
  }

  if (((port->tfd >> AHCI_PxTFD_ERR) & 0xFF) == 1) {
    printk(KERN_DEBG, "AHCI: (0x%x:%d) port error (TFD_ERR)\n", port, slot);
    return false;
  }

  return true;
}

// issues a command and waits for it to complete
bool ahci_port_issue_cmd(ahci_port_t *port, int8_t slot) {
  // check if the port is busy, if so wait till it's not
  while (ahci_port_is_busy(port))
    continue;

  /*

   * this is the commands issued regitser, each bit represents a slot, we set the slot we using to 1

   * this tells the HBA that the command has been built and is ready to be sent to device
   * then we wait until it's set to 0 again, which indicates the HBA received the FIS for this command

  */
  bit_set(port->ci, slot, 1);

  // wait until the command is completed (and also check task file data for errors)
  while (bit_get(port->ci, slot) != 0) {
    if (!ahci_port_check_tfd(port, slot))
      return false;
  }

  if (!ahci_port_check_tfd(port, slot))
    return false;

  return true;
}
