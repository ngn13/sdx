#pragma once

// singly-linked list macros

#define slist_foreach(head, type) for (type *cur = *head; NULL != cur; cur = cur->next)

#define slist_add(head, entry, type)                                                                                   \
  type *cur = NULL;                                                                                                    \
  if (NULL == (cur = *head))                                                                                           \
    *head = entry;                                                                                                     \
  else {                                                                                                               \
    for (; NULL != cur->next; cur = cur->next)                                                                         \
      ;                                                                                                                \
    cur->next = entry;                                                                                                 \
  }

#define slist_del(head, entry, type)                                                                                   \
  if (NULL != *head) {                                                                                                 \
    for (type *cur = *head; NULL != cur->next; cur = cur->next) {                                                      \
      if (entry != cur->next)                                                                                          \
        continue;                                                                                                      \
      cur->next = cur->next->next;                                                                                     \
      break;                                                                                                           \
    }                                                                                                                  \
  }

#define slist_find(head, entry, comparer, value, type)                                                                 \
  slist_foreach(head, type) {                                                                                          \
    if (!comparer(cur, value))                                                                                         \
      continue;                                                                                                        \
    *entry = cur;                                                                                                      \
    break;                                                                                                             \
  }
