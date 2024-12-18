#pragma once

// singly-linked list macros

#define slist_foreach(head, type) for (type *cur = *head; NULL != cur; cur = cur->next)
#define slist_is_end()            (cur->next == NULL)

#define slist_add(head, entry, type)                                                                                   \
  do {                                                                                                                 \
    type *cur = NULL;                                                                                                  \
    if (NULL == (cur = *head))                                                                                         \
      *head = entry;                                                                                                   \
    else {                                                                                                             \
      for (; NULL != cur->next; cur = cur->next)                                                                       \
        ;                                                                                                              \
      cur->next = entry;                                                                                               \
    }                                                                                                                  \
  } while (0);

#define slist_del(head, entry, type)                                                                                   \
  do {                                                                                                                 \
    if (entry == *head) {                                                                                              \
      *head = (*head)->next;                                                                                           \
    } else if (NULL != *head) {                                                                                        \
      slist_foreach(head, type) {                                                                                      \
        if (entry != cur->next)                                                                                        \
          continue;                                                                                                    \
        cur->next = cur->next->next;                                                                                   \
        break;                                                                                                         \
      }                                                                                                                \
    }                                                                                                                  \
  } while (0);

// doubly-linked list macros

#define dlist_foreach(head, type) for (type *cur = *head; NULL != cur; cur = cur->next)
#define dlist_reveach(tail, type) for (type *cur = *tail; NULL != cur; cur = cur->pre)
#define dlist_is_start()          (cur->pre == NULL)
#define dlist_is_end()            (cur->next == NULL)

#define dlist_add(head, tail, entry)                                                                                   \
  do {                                                                                                                 \
    if (NULL == *tail) {                                                                                               \
      *tail = *head = entry;                                                                                           \
      entry->next = entry->pre = NULL;                                                                                 \
    } else {                                                                                                           \
      (entry)->pre = *tail;                                                                                            \
      *tail = *tail->next = entry;                                                                                     \
    }                                                                                                                  \
  } while (0);

#define dlist_del(head, tail, entry, type)                                                                             \
  do {                                                                                                                 \
    if (entry == *head) {                                                                                              \
      *head      = (*head)->next;                                                                                      \
      *head->pre = NULL;                                                                                               \
    }                                                                                                                  \
    if (entry == *tail) {                                                                                              \
      *tail       = (*tail)->pre;                                                                                      \
      *tail->next = NULL;                                                                                              \
    } else if (NULL != *head) {                                                                                        \
      dlist_foreach(head, type) {                                                                                      \
        if (entry != cur->next)                                                                                        \
          continue;                                                                                                    \
        if (NULL != cur->next->next)                                                                                   \
          cur->next->next->pre = cur->pre;                                                                             \
        cur->next = cur->next->next;                                                                                   \
        break;                                                                                                         \
      }                                                                                                                \
    }                                                                                                                  \
  } while (0);
