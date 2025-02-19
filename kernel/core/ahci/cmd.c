#include "core/ahci.h"
#include "mm/vmm.h"

#include "util/bit.h"
#include "util/mem.h"

#include "types.h"
#include "errno.h"

#define ahci_cmd_debg(f, ...) ahci_debg("(0x%x) " f, cmd->port, ##__VA_ARGS__)
#define ahci_cmd_info(f, ...) ahci_info("(0x%x) " f, cmd->port, ##__VA_ARGS__)
#define ahci_cmd_fail(f, ...) ahci_fail("(0x%x) " f, cmd->port, ##__VA_ARGS__)
#define ahci_cmd_warn(f, ...) ahci_warn("(0x%x) " f, cmd->port, ##__VA_ARGS__)

int32_t ahci_cmd_setup(ahci_cmd_t *cmd) {
  if (NULL == cmd)
    return -EINVAL;

  uint64_t size = cmd->data_size;
  void    *data = cmd->data;
  uint8_t  i    = 0;

  // clear the outputs
  cmd->slot   = -1;
  cmd->header = NULL;
  cmd->table  = NULL;

  /*

   * look for an available command slot and obtain the
   * virtual address of the header in the slot

  */
  for (i = 0; i < AHCI_PORT_CMD_LIST_COUNT; i++) {
    if (bit_get(cmd->port->sact, i) || bit_get(cmd->port->ci, i))
      continue;

    cmd->slot   = i;
    cmd->header = ((struct ahci_cmd_header *)cmd->vaddr) + cmd->slot;
    break;
  }

  if (NULL == cmd->header) {
    ahci_cmd_debg("failed to find an available command header for the port");
    return -EFAULT;
  }

  /*

   * setup the fields in the command header, and obtain the
   * command table's virtual address, using CTBA physical pointer

  */
  if (cmd->fis_size % sizeof(uint32_t) != 0) {
    ahci_cmd_debg("invalid FIS size for the command header: %U", cmd->fis_size);
    return -EFAULT;
  }

  cmd->header->cfl   = cmd->fis_size / sizeof(uint32_t);
  cmd->header->prdtl = ahci_prdtl_from_size(size);

  cmd->table = (void *)cmd->vaddr + (cmd->header->ctba - cmd->port->clb);
  bzero(cmd->table, sizeof(struct ahci_cmd_table));

  // setup all the PRDs
  for (i = 0; i < cmd->header->prdtl; i++) {
    cmd->table->prdt[i].interrupt = 0; // don't send an interrupt when the data block transfer is completed
    cmd->table->prdt[i].dba       = vmm_resolve(data); // set the data block base address

    // set the data block size (1 means 2, so we subtract one)
    if (i == cmd->header->prdtl - 1)
      // last PRD's data block size is calculated from left over sector count
      cmd->table->prdt[i].dbc = size - 1;
    else
      // other PRD's just use the data block with the max size
      cmd->table->prdt[i].dbc = AHCI_PRD_DATA_MAX - 1;

    data += AHCI_PRD_DATA_MAX; // the next buffer address
    size -= AHCI_PRD_DATA_MAX; // left over size
  }

  return 0;
}

// issues a command and waits for it to complete
int32_t ahci_cmd_issue(ahci_cmd_t *cmd) {
  // check if the port is busy, if so wait till it's not
  while (ahci_port_is_busy(cmd->port))
    continue;

  /*

   * this is the commands issued regitser, each bit represents a slot, we set the slot we using to 1

   * this tells the HBA that the command has been built and is ready to be sent to device
   * then we wait until it's set to 0 again, which indicates the HBA received the FIS for this command

  */
  bit_set(cmd->port->ci, cmd->slot, 1);

  // wait until the command is completed (and also check task file data for errors)
  while (bit_get(cmd->port->ci, cmd->slot) != 0) {
    if (!ahci_port_check_error(cmd->port, cmd->slot))
      return -EIO;
  }

  // when the command is completed, check for error one last time
  return ahci_port_check_error(cmd->port, cmd->slot) ? 0 : -EIO;
}
