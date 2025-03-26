#include "core/acpi.h"
#include "util/bit.h"

#define FADT_SIG "FACP"

struct fadt {
  uint32_t firmware_ctrl;        // physical address of the FACS
  uint32_t dsdt;                 // physical address of the DSDT
  uint8_t  int_model;            // interrupt model of the ACPI description (ACPI 1.0 only)
  uint8_t  preferred_pm_profile; // OEM's preferred power management profile
  uint16_t sci_int;              // SCI interrupt vector
  uint32_t smi_cmd;              // port address of the SMI command port
  uint8_t  acpi_enable;          // value used to disable ownership of the ACPI registers
  uint8_t  acpi_disable;         // value used to re-enable SMI onwership of the ACPI registers
  uint8_t  s4bios_req;           // value used to enter the S4BIOS state
  uint8_t  pstate_cnt;           // some bullshit i dont understand
  uint32_t pm1a_evt_blk;         // port address of the Power Managment 1a Event Register Block
  uint32_t pm1b_evt_blk;         // same shit
  uint32_t pm1a_cnt_blk;         // port address of the Power Managment 1a Control Register Block
  uint32_t pm1b_cnt_blk;         // again, same shit
  uint32_t pm2_cnt_blk;          // same shit but optional (contains zero if not supported)
  uint32_t pm_timer_block;       // port address of the Power Managment Timer Control Register Block
  uint32_t gpe0_blk;             // port address of Generic Purpose Event 0 Register Block
  uint32_t gpe1_blk;             // same shit
  uint8_t  pm1_evt_len;          // number of bytes decoded by PM1a event block
  uint8_t  pm1_cnt_len;          // you get it
  uint8_t  pm2_cnt_len;          // it's the same fucking thing
  uint8_t  pm_timer_len;         // for the other registers
  uint8_t  gpe0_len;             // that's probably why they added GAS
  uint8_t  gpe1_len;             // bc it's same fucking shit
  uint8_t  gpe1_base;            // offset where GPE1 based events start
  uint8_t  cst_cnt;              // some weird shit
  uint16_t p_lvl2_lat;           // worst-case hw latency to enter and exit a C2 state (ms, >100 means no C2 state)
  uint16_t p_lvl3_lat;           // same shit for C3 state (this time >1000 means no C3 state)

  // some random bullshit
  uint16_t flush_size;
  uint16_t flush_stride;
  uint8_t  duty_offset;
  uint8_t  duty_width;

  // some CMOS shit
  uint8_t day_alarm;
  uint8_t month_alarm;
  uint8_t century;

  uint16_t iapc_boot_arch; // IA-PC Boot Architecture Flags (ACPI 2.0 only)
  uint8_t  reserved2;
  uint32_t flags; // fixed feature flags

  // rest of the fields only exist in ACPI 2.0 (and later)
  acpi_gas_t reset_reg;          // address of the reset register, in the GAS format
  uint8_t    reset_value;        // value used to reset the system
  uint16_t   arm_boot_arch;      // ARM Boot Architecture Flags
  uint8_t    fadt_minor_version; // minor version of this FADT structure

  /*

   * these are just the GAS structured or 64 bit versions of the previous
   * HW registers and addresses, only available in ACPI 2.0+

   * should check and use these instead of the original ones, if old ones are
   * zero (or "NULL") we use the new ones, and if the new ones are zero we use
   * the old ones

  */
  uint64_t   x_firmware_control;
  uint64_t   x_dsdt;
  acpi_gas_t x_pm1a_event_block;
  acpi_gas_t x_pm1b_event_block;
  acpi_gas_t x_pm1a_control_block;
  acpi_gas_t x_pm1b_control_block;
  acpi_gas_t x_pm2_control_block;
  acpi_gas_t x_pm_timer_block;
  acpi_gas_t x_gpe0_block;
  acpi_gas_t x_gpe1_block;
};

// 5.2.9.3 IA-PC Boot Architecture Flags
enum {
  IAPC_BOOT_LEGACY_DEVICES,
  IAPC_BOOT_8042,
  IAPC_BOOT_VGA_NOT_PRESENT,
  IAPC_BOOT_MSI_NOT_SUPPORTED,
  IAPC_BOOT_PCIE_ASPM_CONTROLS,
  IAPC_BOO_CMOS_RTC_NOT_PRESENT,
};

bool acpi_supports_8042_ps2() {
  // get the FADT and the ACPI version
  struct fadt *fadt    = acpi_find(FADT_SIG, sizeof(struct fadt));
  int32_t      version = acpi_version();

  // if ACPI 2.0 is not supported then its supported
  if (version < ACPI_VERSION_2 || NULL == fadt)
    return true;

  // otherwise we check the "8042" field of the IA-PC boot flags
  return bit_get(fadt->iapc_boot_arch, IAPC_BOOT_8042);
}
