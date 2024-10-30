#include "util/path.h"
#include "errno.h"
#include "util/mem.h"
#include "util/string.h"

#define __path_clear(o) (bzero(o, PATH_MAX + 1))
#define __path_check_psize(s)                                                                                          \
  if (s > PATH_MAX)                                                                                                    \
  return -ENAMETOOLONG
#define __path_check_nsize(s)                                                                                          \
  if (s > NAME_MAX)                                                                                                    \
  return -ENAMETOOLONG

int32_t path_sanitize(char out[PATH_MAX + 1], char *in) {
  return -ENOSYS;
}

int32_t path_getcwd(char out[PATH_MAX + 1]) {
  // CWD not yet implemented
  out[0] = '/';
  out[1] = 0;

  return 1;
}
