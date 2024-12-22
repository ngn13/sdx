#include "util/string.h"
#include "util/math.h"
#include "util/mem.h"

#include "errno.h"
#include "video.h"

// error_info and error_table is taken from GCC (libiberty/strerror.c)

struct error_info {
  const int32_t     value;
  const char *const name;
  const char *const msg;
};

#define ENTRY(value, name, msg) {value, name, msg}

struct error_info error_table[] = {ENTRY(EPERM, "EPERM", "Not owner"),
    ENTRY(ENOENT, "ENOENT", "No such file or directory"),
    ENTRY(ESRCH, "ESRCH", "No such process"),
    ENTRY(EINTR, "EINTR", "Interrupted system call"),
    ENTRY(EIO, "EIO", "I/O error"),
    ENTRY(ENXIO, "ENXIO", "No such device or address"),
    ENTRY(E2BIG, "E2BIG", "Arg list too long"),
    ENTRY(ENOEXEC, "ENOEXEC", "Exec format error"),
    ENTRY(EBADF, "EBADF", "Bad file number"),
    ENTRY(ECHILD, "ECHILD", "No child processes"),
    ENTRY(EWOULDBLOCK, "EWOULDBLOCK", "Operation would block"),
    ENTRY(EAGAIN, "EAGAIN", "No more processes"),
    ENTRY(ENOMEM, "ENOMEM", "Not enough space"),
    ENTRY(EACCES, "EACCES", "Permission denied"),
    ENTRY(EFAULT, "EFAULT", "Bad address"),
    ENTRY(ENOTBLK, "ENOTBLK", "Block device required"),
    ENTRY(EBUSY, "EBUSY", "Device busy"),
    ENTRY(EEXIST, "EEXIST", "File exists"),
    ENTRY(EXDEV, "EXDEV", "Cross-device link"),
    ENTRY(ENODEV, "ENODEV", "No such device"),
    ENTRY(ENOTDIR, "ENOTDIR", "Not a directory"),
    ENTRY(EISDIR, "EISDIR", "Is a directory"),
    ENTRY(EINVAL, "EINVAL", "Invalid argument"),
    ENTRY(ENFILE, "ENFILE", "File table overflow"),
    ENTRY(EMFILE, "EMFILE", "Too many open files"),
    ENTRY(ENOTTY, "ENOTTY", "Not a typewriter"),
    ENTRY(ETXTBSY, "ETXTBSY", "Text file busy"),
    ENTRY(EFBIG, "EFBIG", "File too large"),
    ENTRY(ENOSPC, "ENOSPC", "No space left on device"),
    ENTRY(ESPIPE, "ESPIPE", "Illegal seek"),
    ENTRY(EROFS, "EROFS", "Read-only file system"),
    ENTRY(EMLINK, "EMLINK", "Too many links"),
    ENTRY(EPIPE, "EPIPE", "Broken pipe"),
    ENTRY(EDOM, "EDOM", "Math argument out of domain of func"),
    ENTRY(ERANGE, "ERANGE", "Result too large"), // modified, original message: "Math result not representable"
    ENTRY(ENOMSG, "ENOMSG", "No message of desired type"),
    ENTRY(EIDRM, "EIDRM", "Identifier removed"),
    ENTRY(ECHRNG, "ECHRNG", "Channel number out of range"),
    ENTRY(EL2NSYNC, "EL2NSYNC", "Level 2 not synchronized"),
    ENTRY(EL3HLT, "EL3HLT", "Level 3 halted"),
    ENTRY(EL3RST, "EL3RST", "Level 3 reset"),
    ENTRY(ELNRNG, "ELNRNG", "Link number out of range"),
    ENTRY(EUNATCH, "EUNATCH", "Protocol driver not attached"),
    ENTRY(ENOCSI, "ENOCSI", "No CSI structure available"),
    ENTRY(EL2HLT, "EL2HLT", "Level 2 halted"),
    ENTRY(EDEADLK, "EDEADLK", "Deadlock condition"),
    ENTRY(ENOLCK, "ENOLCK", "No record locks available"),
    ENTRY(EBADE, "EBADE", "Invalid exchange"),
    ENTRY(EBADR, "EBADR", "Invalid request descriptor"),
    ENTRY(EXFULL, "EXFULL", "Exchange full"),
    ENTRY(ENOANO, "ENOANO", "No anode"),
    ENTRY(EBADRQC, "EBADRQC", "Invalid request code"),
    ENTRY(EBADSLT, "EBADSLT", "Invalid slot"),
    ENTRY(EDEADLOCK, "EDEADLOCK", "File locking deadlock error"),
    ENTRY(EBFONT, "EBFONT", "Bad font file format"),
    ENTRY(ENOSTR, "ENOSTR", "Device not a stream"),
    ENTRY(ENODATA, "ENODATA", "No data available"),
    ENTRY(ETIME, "ETIME", "Timer expired"),
    ENTRY(ENOSR, "ENOSR", "Out of streams resources"),
    ENTRY(ENONET, "ENONET", "Machine is not on the network"),
    ENTRY(ENOPKG, "ENOPKG", "Package not installed"),
    ENTRY(EREMOTE, "EREMOTE", "Object is remote"),
    ENTRY(ENOLINK, "ENOLINK", "Link has been severed"),
    ENTRY(EADV, "EADV", "Advertise error"),
    ENTRY(ESRMNT, "ESRMNT", "Srmount error"),
    ENTRY(ECOMM, "ECOMM", "Communication error on send"),
    ENTRY(EPROTO, "EPROTO", "Protocol error"),
    ENTRY(EMULTIHOP, "EMULTIHOP", "Multihop attempted"),
    ENTRY(EDOTDOT, "EDOTDOT", "RFS specific error"),
    ENTRY(EBADMSG, "EBADMSG", "Not a data message"),
    ENTRY(ENAMETOOLONG, "ENAMETOOLONG", "File name too long"),
    ENTRY(EOVERFLOW, "EOVERFLOW", "Value too large for defined data type"),
    ENTRY(ENOTUNIQ, "ENOTUNIQ", "Name not unique on network"),
    ENTRY(EBADFD, "EBADFD", "File descriptor in bad state"),
    ENTRY(EREMCHG, "EREMCHG", "Remote address changed"),
    ENTRY(ELIBACC, "ELIBACC", "Cannot access a needed shared library"),
    ENTRY(ELIBBAD, "ELIBBAD", "Accessing a corrupted shared library"),
    ENTRY(ELIBSCN, "ELIBSCN", ".lib section in a.out corrupted"),
    ENTRY(ELIBMAX, "ELIBMAX", "Attempting to link in too many shared libraries"),
    ENTRY(ELIBEXEC, "ELIBEXEC", "Cannot exec a shared library directly"),
    ENTRY(EILSEQ, "EILSEQ", "Illegal byte sequence"),
    ENTRY(ENOSYS, "ENOSYS", "Function not implemented"), // modified, original message: "Operation not applicable"
    ENTRY(ELOOP, "ELOOP", "Too many symbolic links encountered"),
    ENTRY(ERESTART, "ERESTART", "Interrupted system call should be restarted"),
    ENTRY(ESTRPIPE, "ESTRPIPE", "Streams pipe error"),
    ENTRY(ENOTEMPTY, "ENOTEMPTY", "Directory not empty"),
    ENTRY(EUSERS, "EUSERS", "Too many users"),
    ENTRY(ENOTSOCK, "ENOTSOCK", "Socket operation on non-socket"),
    ENTRY(EDESTADDRREQ, "EDESTADDRREQ", "Destination address required"),
    ENTRY(EMSGSIZE, "EMSGSIZE", "Message too long"),
    ENTRY(EPROTOTYPE, "EPROTOTYPE", "Protocol wrong type for socket"),
    ENTRY(ENOPROTOOPT, "ENOPROTOOPT", "Protocol not available"),
    ENTRY(EPROTONOSUPPORT, "EPROTONOSUPPORT", "Protocol not supported"),
    ENTRY(ESOCKTNOSUPPORT, "ESOCKTNOSUPPORT", "Socket type not supported"),
    ENTRY(EOPNOTSUPP, "EOPNOTSUPP", "Operation not supported on transport endpoint"),
    ENTRY(EPFNOSUPPORT, "EPFNOSUPPORT", "Protocol family not supported"),
    ENTRY(EAFNOSUPPORT, "EAFNOSUPPORT", "Address family not supported by protocol"),
    ENTRY(EADDRINUSE, "EADDRINUSE", "Address already in use"),
    ENTRY(EADDRNOTAVAIL, "EADDRNOTAVAIL", "Cannot assign requested address"),
    ENTRY(ENETDOWN, "ENETDOWN", "Network is down"),
    ENTRY(ENETUNREACH, "ENETUNREACH", "Network is unreachable"),
    ENTRY(ENETRESET, "ENETRESET", "Network dropped connection because of reset"),
    ENTRY(ECONNABORTED, "ECONNABORTED", "Software caused connection abort"),
    ENTRY(ECONNRESET, "ECONNRESET", "Connection reset by peer"),
    ENTRY(ENOBUFS, "ENOBUFS", "No buffer space available"),
    ENTRY(EISCONN, "EISCONN", "Transport endpoint is already connected"),
    ENTRY(ENOTCONN, "ENOTCONN", "Transport endpoint is not connected"),
    ENTRY(ESHUTDOWN, "ESHUTDOWN", "Cannot send after transport endpoint shutdown"),
    ENTRY(ETOOMANYREFS, "ETOOMANYREFS", "Too many references: cannot splice"),
    ENTRY(ETIMEDOUT, "ETIMEDOUT", "Connection timed out"),
    ENTRY(ECONNREFUSED, "ECONNREFUSED", "Connection refused"),
    ENTRY(EHOSTDOWN, "EHOSTDOWN", "Host is down"),
    ENTRY(EHOSTUNREACH, "EHOSTUNREACH", "No route to host"),
    ENTRY(EALREADY, "EALREADY", "Operation already in progress"),
    ENTRY(EINPROGRESS, "EINPROGRESS", "Operation now in progress"),
    ENTRY(ESTALE, "ESTALE", "Stale NFS file handle"),
    ENTRY(EUCLEAN, "EUCLEAN", "Structure needs cleaning"),
    ENTRY(ENOTNAM, "ENOTNAM", "Not a XENIX named type file"),
    ENTRY(ENAVAIL, "ENAVAIL", "No XENIX semaphores available"),
    ENTRY(EISNAM, "EISNAM", "Is a named type file"),
    ENTRY(EREMOTEIO, "EREMOTEIO", "Remote I/O error"),
    ENTRY(0, NULL, NULL)};

uint8_t strcmp(char *s1, char *s2) {
  if (NULL == s1 || NULL == s2)
    return -1;

  for (; *s1 != 0 && *s2 != 0; s1++, s2++) {
    if (*s1 != *s2)
      break;
  }

  return s1[0] - s2[0];
}

uint64_t strncmp(char *s1, char *s2, uint64_t len) {
  for (; len > 0; len--) {
    if (*s1 == 0)
      return 0;

    if (*s1 != *s2)
      return *s1 - *s2;

    s1++;
    s2++;
  }

  return 0;
}

char *strlwr(char *str) {
  for (; *str != 0; str++)
    *str |= 32;
  return str;
}

uint64_t strlen(const char *str) {
  if (NULL == str)
    return 0;

  uint64_t len = 0;
  while (str[len] != 0)
    len++;
  return len;
}

bool strrev(char *str) {
  if (NULL == str)
    return false;

  uint64_t indx  = strlen(str);
  char    *strcp = str;
  char     c     = 0;

  if (indx <= 1)
    return true;
  indx--;

  while (str != strcp + indx) {
    c           = *str;
    *str        = strcp[indx];
    strcp[indx] = c;

    str++;

    if (str == strcp + indx)
      break;

    indx--;
  }

  return true;
}

uint64_t itou(uint64_t val, char *dst) {
  if (NULL == dst)
    return NULL;

  uint64_t num = abs(val), i = 0;

  do {
    dst[i++] = (num % 10) + '0';
    num /= 10;
  } while (num != 0);

  dst[i++] = 0;

  strrev(dst);
  return i - 1;
}

uint64_t itod(int64_t val, char *dst) {
  if (NULL == dst)
    return NULL;

  uint64_t num = abs(val), i = 0;

  do {
    dst[i++] = (num % 10) + '0';
    num /= 10;
  } while (num != 0);

  if (val < 0)
    dst[i++] = '-';
  dst[i++] = 0;

  strrev(dst);
  return i - 1;
}

uint64_t itoh(uint64_t val, char *dst) {
  if (NULL == dst)
    return NULL;

  char     hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  uint64_t num = abs(val), i = 0;

  do {
    dst[i++] = hex_chars[(num % 16)];
    num /= 16;
  } while (num != 0);

  dst[i++] = 0;

  strrev(dst);
  return i - 1;
}

char *strchr(char *s, char c) {
  if (NULL == s)
    return NULL;

  for (; *s != 0; s++)
    if (*s == c)
      return s;

  return NULL;
}

char *strstr(char *s1, char *s2) {
  char    *p   = s1;
  uint64_t len = strlen(s2);

  if (len == 0)
    return s1;

  for (; (p = strchr(p, *s2)) != 0; p++)
    if (strncmp(p, s2, len) == 0)
      return p;

  return NULL;
}

char *strncpy(char *dst, const char *src, uint64_t dsize) {
  uint64_t ssize = strlen(src) + 1;

  if (dsize > ssize)
    dsize = ssize;

  dst  = memcpy(dst, (void *)src, dsize - 1);
  *dst = 0;

  return dst;
}

const char *strerror(int32_t err) {
  struct error_info *cur = error_table;

  for (; cur->value != 0; cur++)
    if (err == cur->value || err == -cur->value)
      return cur->msg;

  return "Unknown error code";
}
