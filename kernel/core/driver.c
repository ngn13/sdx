#include "core/driver.h"
#include "core/serial.h"
#include "core/acpi.h"
#include "core/ps2.h"
#include "core/pci.h"
#include "core/kbd.h"

#include "util/string.h"
#include "util/list.h"

#include "types.h"
#include "errno.h"

#define driver_info(f, ...) pinfo("%s driver: " f, driver_cur->name, ##__VA_ARGS__)
#define driver_fail(f, ...) pfail("%s driver: " f, driver_cur->name, ##__VA_ARGS__)
#define driver_debg(f, ...) pdebg("%s driver: " f, driver_cur->name, ##__VA_ARGS__)

driver_t *drivers[] = {
    &serial_driver, // core/serial.c
    &acpi_driver,   // core/acpi.c
    &pci_driver,    // core/pci.c
    &ps2_driver,    // core/ps2.c
    &kbd_driver,    // core/kbd.c
    NULL,
};

#define driver_foreach(list) for (driver_t **cur = &list[0]; NULL != *cur; cur++)
#define driver_cur           (*cur)

bool __driver_depends_loaded(driver_t *driver) {
  driver_foreach(driver->depends) {
    if (!driver_cur->loaded)
      return false;
  }

  return true;
}

void drivers_load() {
  int32_t err = 0;

  driver_foreach(drivers) {
    driver_cur->loaded = false;

    // check if all the depends are loaded
    if (!__driver_depends_loaded(driver_cur))
      continue;

    if (NULL != driver_cur->load && (err = driver_cur->load()) != 0) {
      driver_fail("failed to load: %s", strerror(err));
      continue;
    }

    // success
    driver_cur->loaded = true;
  }
}

void drivers_unload() {
  int32_t err = 0;

  driver_foreach(drivers) {
    if (!driver_cur->loaded)
      continue;

    if (NULL != driver_cur->unload && (err = driver_cur->unload()) != 0)
      driver_fail("failed to unload: %s", strerror(err));

    driver_cur->loaded = false;
  }
}
