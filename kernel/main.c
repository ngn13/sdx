// clang-format off

/*

 * sdx | simple and dirty UNIX
 * written by ngn (https://ngn.tf) (2024)

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

// clang-format on

#include "core/im.h"
#include "core/pci.h"
#include "core/pic.h"
#include "core/serial.h"

#include "boot/end.h"
#include "boot/multiboot.h"

#include "mm/pm.h"
#include "mm/vmm.h"

#include "fs/vfs.h"
#include "video.h"

#include "util/panic.h"
#include "util/printk.h"

void entry() {
  /*

   * here, we initialize video memory in width the framebuffer mode
   * framebuffer info is obtained from the multiboot data
   * see boot/multiboot.S

   * also cars 2 is the best cars movie btw

  */
  if (!video_init(VIDEO_MODE_FRAMEBUFFER))
    panic(__func__, "Failed to initialize the framebuffer video mode");

  // initialize serial communication ports (UART)
  serial_init();

  /*

   * obtain the available memory region start and the end address

   * for the start: we check if the page table's end address is larger then
   * the end address for the kernel, if so, we use the page table's end address

   * for the end: we check if the last mapped memory address is larger then the
   * available memory region's end address, if so, we use the region's address

  */
  uint64_t avail_start = pm_end > end_addr ? pm_end : end_addr;
  uint64_t avail_end   = pm_mapped > mb_mem_avail_limit ? mb_mem_avail_limit : pm_mapped;

  /*

   * paging is done in before switching to long mode
   * here we are just doing some extra checks and logging

   * see boot/paging.S and boot/multiboot.S

  */
  if (!pm_init(avail_start, avail_end))
    panic(__func__, "Failed to initialize the paging manager");

  /*

   * initialize the interrupt manager (IM)

   * by default all the interrupts are handled by the default handler
   * this can be changed using the other IM functions

  */
  im_init();

  /*

   * initialize the programmable interrupt controller (PIC)

   * we need to enable this before enabling interrupts
   * otherwise since the vector offset is not set we would get a random
   * exception interrupt from the PIC

  */
  if (!pic_init())
    panic(__func__, "Failed to initialize the PIC");

  if (!pic_enable())
    panic(__func__, "Failed to enable the PIC");

  if (!pic_mask(PIC_IRQ_TIMER))
    panic(__func__, "Failed to mask timer IRQ");

  // enable the interrupts
  im_enable();

  // initialize peripheral component interconnect (PCI) devices
  pci_init();

  // mount any potential root VFS
  if (NULL == vfs_detect())
    panic(__func__, "No available root partition");

  /*

   * this is where we are gonna start the init program and wait for userland calls
   * for now tho, we'll just print a hello world message and return to halt the system (see boot/loader.S)

  */
  while (true)
    continue;

  return;
}
