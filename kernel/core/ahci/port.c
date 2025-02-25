#include "core/ahci.h"

#include "mm/vmm.h"
#include "mm/pmm.h"

#include "util/bit.h"
#include "util/mem.h"
#include "util/printk.h"

#include "types.h"
#include "errno.h"

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
  struct sata_fis_pio_setup psfis;
  uint8_t                   padding1[12];

  // D2H register FIS
  struct sata_fis_d2h rfis;
  uint8_t             padding2[4];

  // set device bits FIS
  uint8_t sdbfis[8];

  // unknown FIS
  uint8_t ufis[64];

  uint8_t reserved[96];
};

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

// starts the stoppen HBA back again
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

 * checks the DET, IPM and the signature of the port to see if it is connected to a
 * supported device, if it's then returns true, if not returns false

*/
bool ahci_port_is_connected(ahci_port_t *port) {
  uint8_t det = port->ssts & 0x0F;       // device detection (bits 0-3)
  uint8_t ipm = (port->ssts >> 8) & 0xF; // interface power managment (bits 8-11)

  if (det != AHCI_PxSSTS_DET_OK || ipm != AHCI_PxSSTS_IPM_ACTIVE)
    return false;

  /*

   * reset da port bc signature is not correct for some fucking reason
   * maybe the first D2H is not received yet or something idfk

  */
  if (!ahci_port_reset(port)) {
    printk(KERN_DEBG, "AHCI: failed to reset port 0x%p during check\n", port);
    return false;
  }

  return port->sig == AHCI_SIGNATURE_SATA || port->sig == AHCI_SIGNATURE_ATAPI;
}

/*

 * allocates and sets up command list and recevied FIS structure which are
 * pointed by PxCLB and PxFB, respectively

 * for more info see "Figure 5: Port System Memory Structures"

 * returns the virtual address of PxCLB which is the base address that will
 * be used to calculate other data structures virtual addresses (yes all the
 * data structures store only the physical addresses)

*/
void *ahci_port_setup(ahci_port_t *port) {
  // stop sending SATA commands to the device
  if (!ahci_port_stop(port)) {
    ahci_fail("failed to stop port 0x%p for initialization", port);
    return NULL;
  }

  // virtual address for the command list base address
  void *clb_vaddr = 0;

  // calculate the size required for the command list and the 256 byte aligned received FIS
  uint64_t command_table_offset[AHCI_PORT_CMD_LIST_COUNT] = {0};
  uint64_t received_fis_offset                            = 0;

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
  for (uint8_t i = 0; i < AHCI_PORT_CMD_LIST_COUNT; i++) {
    // offset to make sure the command table is 128 byte aligned
    while (size % 128 != 0)
      size++;
    command_table_offset[i] = size;
    size += sizeof(struct ahci_cmd_table);
  }

  page_count = vmm_calc(size);

  /*

   * each port has a CLB (Command List Base Address) which points to a command list
   * and the command list is basically a list of 32 ahci_cmd_headers

   * the intereseting thing about the command list is that it needs to be a 1024 byte
   * aligned address as the lower 10 bits of the address is reserved for some fucking
   * reason

  */
  clb_vaddr = vmm_map(page_count, 1024, VMM_ATTR_NO_CACHE);
  port->clb = vmm_resolve(clb_vaddr);
  bzero(clb_vaddr, size);

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
  struct ahci_cmd_header *header = (void *)clb_vaddr;

  for (uint8_t i = 0; i < AHCI_PORT_CMD_LIST_COUNT; i++) {
    header[i].prdtl = AHCI_PRDTL_MAX;
    header[i].ctba  = port->clb + command_table_offset[i];
  }

  // clear interrupt status and enable all interrupts
  port->is = UINT32_MAX;
  port->ie = 1;

  if (!ahci_port_start(port)) {
    ahci_fail("failed to start port 0x%p after initialization", port);
    return NULL;
  }

  return clb_vaddr;
}

// check if TFD register contains an error, if so return false
bool ahci_port_check_error(ahci_port_t *port, int64_t slot) {
  if (bit_get(port->tfd, AHCI_PxTFD_STS_ERR) == 1) {
    ahci_debg("transfer error (TFD_STST_ERR) for port 0x%p, slot: %d", port, slot);
    return false;
  }

  if (((port->tfd >> AHCI_PxTFD_ERR) & 0xFF) == 1) {
    ahci_debg("port error (TFD_ERR) for port 0x%p, slot: %d", port, slot);
    return false;
  }

  return true;
}

// check if the port is busy by checking the TFD register
bool ahci_port_is_busy(ahci_port_t *port) {
  return bit_get(port->tfd, AHCI_PxTFD_STS_BSY) || bit_get(port->tfd, AHCI_PxTFD_STS_DRQ);
}
