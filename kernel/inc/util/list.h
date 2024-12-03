#pragma once

// singly-linked list macros

#define slist_foreach(h, t) for (t *p = *h; NULL != p; p = p->next)

#define slist_add(h, e, t)                                                                                             \
  t *p = NULL;                                                                                                         \
  if (NULL == (p = *h))                                                                                                \
    *h = e;                                                                                                            \
  else {                                                                                                               \
    for (; NULL != p->next; p = p->next)                                                                               \
      ;                                                                                                                \
    p->next = e;                                                                                                       \
  }

#define slist_del(h, e, t)                                                                                             \
  if (NULL != *h) {                                                                                                    \
    for (t *p = *h; NULL != p->next; p = p->next) {                                                                    \
      if (e != p->next)                                                                                                \
        continue;                                                                                                      \
      p->next = p->next->next;                                                                                         \
    }                                                                                                                  \
  }

#define slist_find(h, e, c, v, t)                                                                                      \
  slist_foreach(h, t) {                                                                                                \
    if (c(p, v) != 0)                                                                                                  \
      continue;                                                                                                        \
    *e = p;                                                                                                            \
  }
