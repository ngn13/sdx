#include "core/timer.h"
#include "core/pic.h"
#include "core/im.h"

#include "sched/sched.h"
#include "sched/task.h"

#include "util/math.h"
#include "util/io.h"

#include "types.h"
#include "errno.h"

#define timer_debg(f, ...) pdebg("Timer: " f, ##__VA_ARGS__)
#define timer_warn(f, ...) pwarn("Timer: " f, ##__VA_ARGS__)
#define timer_fail(f, ...) pfail("Timer: " f, ##__VA_ARGS__)

/*

 * PIT has few ports, first it has 3 R/W ports for the 3 channels
 * but a part from that it also has a separate write-only channel
 * just for commands and setting operating modes

 * now the only channel we are interested in is the channel 0, which
 * is the one that generates the actual timer interrupts, rest is useless
 * and they may not be even present in a modern system

*/
#define PIT_DATA (0x40)          // channel 0 data port
#define PIT_MODE (0x43)          // command/mode
#define PIT_FREQ (1193180)       // frequency
#define PIT_HZ   (100)           // hz (interrupt n times per sec)
#define PIT_IPMS (1000 / PIT_HZ) // interrupts per ms

// runs on every timer interrupt to handle stuff like sleep
void __timer_handler() {
  sched_foreach() {
    if (cur->sleep > 0)
      cur->sleep--;
    else
      sched_unblock(cur, TASK_BLOCK_SLEEP);
  }
}

int32_t timer_init() {
  // calculate the countdown value
  uint16_t count = div_ceil(PIT_FREQ, PIT_HZ);

  // disable interrupts while modifying the count
  im_disable();

  /*

   * bit 0  : selects the binary mode, 2 modes for sending data through the
   *          data ports, first mode is the binary mode (0), where you just
   *          send the 16 bit binary encoded data, like for example to send
   *          42, you send 0b101010 - the other mode is BCD (1, binary coded
   *          decimal) where you send 4 bits for each digit, so to send 42
   *          you would send 0b0010 and then 0b0100 (so "4" and "2"), we set
   *          this to 0 to use the binary mode (0)

   * bit 1-3: this is the operating mode, on the osdev wiki page, all the modes
   *          are explained really well and detailed with all the nerdy details
   *          that literally no one gives a single fuck about so feel free to
   *          visit that PIT page and laugh about the fact that some fucking
   *          loser actually typed that shit out - but basically the only modes
   *          we care about is 0, 2 and 3. 0 is like a one shot mode, where it
   *          counts down from a given number, and when it reaches zero it
   *          triggers an interrupt until you give it an anther number to count
   *          down from, mode 2 is a periodic counter, it counts down from the
   *          number provided, and when it reaches 0, it triggers an interrupt,
   *          and starts counting down from the same number again, lastly 3 is
   *          a less precise version of the mode 2, so we'll use mode 2 for our
   *          timer (010)

   * bit 4-5: sets the access mode, basically tells how do we want to read/write
   *          from/to the data port, mode 3 first reads/writes the LSB and then
   *          the MSB of the 16 bits, so thats what we are going to use (11)

   * bit 6-7: lasty, this is used to specify the channel, as I said, channel 0 is
   *          used to generate the timer interrupt, so we'Ll use that (00)

  */
  out8(PIT_MODE, 0b00110100);

  // write the divisor (countdown value)
  out8(PIT_DATA, count & 0xff);
  out8(PIT_DATA, count >> 8);

  // enable the interrupts again
  im_enable();

  // register timer handler
  im_add_handler(pic_to_int(PIC_IRQ_TIMER), __timer_handler);

  // success
  timer_debg("now running with %uHz", PIT_HZ);
  return 0;
}

int32_t timer_sleep(uint64_t ms) {
  if (NULL == current) {
    timer_fail("attempt to sleep before scheduler initialization");
    return -EINVAL;
  }

  // check if the requested sleep amount is to small
  if (PIT_IPMS > ms) {
    timer_warn("attempt to sleep under %u, ignoring");
    sched_sleep(1);
  }

  // calculate how many ticks we need to sleep for
  else {
    uint64_t ticks = round_up(ms, 10) / PIT_IPMS;
    sched_sleep(ticks);
  }

  // block task until sleep is complete
  sched_block_until(TASK_BLOCK_SLEEP, current->sleep != 0);

  // sleep is complete
  return 0;
}
