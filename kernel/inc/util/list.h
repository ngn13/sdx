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
  } while (0)

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
  } while (0)

#define slist_clear(head, free, type)                                                                                  \
  do {                                                                                                                 \
    type *cur = *head, *next = NULL;                                                                                   \
    while (NULL != cur) {                                                                                              \
      next = cur->next;                                                                                                \
      free(cur);                                                                                                       \
      cur = next;                                                                                                      \
    }                                                                                                                  \
    *head = NULL;                                                                                                      \
  } while (0)

// doubly-linked list macros

#define dlist_foreach(head, type) for (type *cur = *head; NULL != cur; cur = cur->next)
#define dlist_reveach(tail, type) for (type *cur = *tail; NULL != cur; cur = cur->prev)
#define dlist_is_start()          (cur->prev == NULL)
#define dlist_is_end()            (cur->next == NULL)

#define dlist_add(head, tail, entry)                                                                                   \
  do {                                                                                                                 \
    if (NULL == *tail) {                                                                                               \
      *tail = *head = entry;                                                                                           \
      entry->next = entry->prev = NULL;                                                                                \
    } else {                                                                                                           \
      (entry)->prev = *tail;                                                                                           \
      *tail = *tail->next = entry;                                                                                     \
    }                                                                                                                  \
  } while (0)

#define dlist_del(head, tail, entry, type)                                                                             \
  do {                                                                                                                 \
    if (entry == *head) {                                                                                              \
      *head       = (*head)->next;                                                                                     \
      *head->prev = NULL;                                                                                              \
    }                                                                                                                  \
    if (entry == *tail) {                                                                                              \
      *tail       = (*tail)->prev;                                                                                     \
      *tail->next = NULL;                                                                                              \
    } else if (NULL != *head) {                                                                                        \
      dlist_foreach(head, type) {                                                                                      \
        if (entry != cur->next)                                                                                        \
          continue;                                                                                                    \
        if (NULL != cur->next->next)                                                                                   \
          cur->next->next->prev = cur->prev;                                                                           \
        cur->next = cur->next->next;                                                                                   \
        break;                                                                                                         \
      }                                                                                                                \
    }                                                                                                                  \
  } while (0)

#define dlist_clear(head, tail, free, type)                                                                            \
  do {                                                                                                                 \
    type *cur = *head, *next = NULL;                                                                                   \
    while (NULL != cur) {                                                                                              \
      next = cur->next;                                                                                                \
      free(cur);                                                                                                       \
      cur = next;                                                                                                      \
    }                                                                                                                  \
    *head = NULL;                                                                                                      \
    *tail = NULL;                                                                                                      \
  } while (0)
