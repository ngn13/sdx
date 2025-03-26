#pragma once
#include "types.h"

#ifndef __ASSEMBLY__

/*

 * useful links:
 * - https://wiki.osdev.org/ACPI
 * - https://uefi.org/sites/default/files/resources/ACPI_Spec_6_4_Jan22.pdf
 * - https://uefi.org/sites/default/files/resources/ACPI_1.pdf

*/

// describes the ACPI version that's in use
enum { ACPI_VERSION_1 = 1, ACPI_VERSION_2 };

// generic address structure
typedef struct {
  uint8_t  addr_space;
  uint8_t  bit_width;
  uint8_t  bit_offset;
  uint8_t  access_size;
  uint64_t addr;
} acpi_gas_t;

// core/acpi/acpi.c
int32_t acpi_load();
void   *acpi_find(char *sig, uint64_t size);
int32_t acpi_version();

// core/acpi/fadt.c
bool acpi_supports_8042_ps2();

#endif
