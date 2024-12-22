#pragma once

#define SIGHUP  (1) // hangup
#define SIGINT  (2) // interrupt
#define SIGILL  (3) // illegal instruction
#define SIGKILL (4) // kill
#define SIGSEGV (5) // segmentation violation

#define SIG_DFL ((void *)0) // use the default handler
#define SIG_IGN ((void *)1) // ignore the signal
