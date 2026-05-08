#define _POSIX_C_SOURCE 200809L
#include "hash.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET_BASIS_64 0xcbf29ce484222325ULL
#define FNV_PRIME_64 0x100000001b3ULL

uint64_t fnv1a(const char *s) {
  uint64_t h = FNV_OFFSET_BASIS_64;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= FNV_PRIME_64;
  }
  return h;
}

kv_table *kv_table_new(size_t cap) {
  if (cap == 0) {
    return NULL;
  }

  kv_table *t = malloc(sizeof(*t));
  if (t == NULL) {
    return NULL;
  }

  t->buckets = calloc(cap, sizeof(*t->buckets));
  if (t->buckets == NULL) {
    free(t);
    return NULL;
  }

  t->cap = cap;
  t->len = 0;
  return t;
}

void kv_table_free(kv_table *t) {
  if (t == NULL) {
    return;
  }

  for ( size_t i = 0; i < t->cap; i++ ) {
    kv_entry *e = t->buckets[i];
    while (e != NULL) {
      kv_entry *next = e->next;
      free(e->key);
      free(e->value);
      free(e);
      e = next; 
    }
  }
  free(t->buckets);
  free(t);
}

int kv_table_set(kv_table *t, const char *key, const char *value) {
  uint64_t h = fnv1a(key); // hash the key
  size_t i = h % t->cap; // pick the bucket

  // walk the chain looking for a a matching key.
  for (kv_entry *e = t->buckets[i]; e != NULL; e = e->next) {
    if (strcmp(e->key, key) == 0) {
      /* Found - update the value */
      char *new_value = strdup(value);
      if (new_value == NULL) {
        return -1;
      }
      free(e->value);
      e->value = new_value;
      return 0;
    };
  }

  /* Not found - allocate a new entry and prepend to the chain */
  kv_entry *e = malloc(sizeof(*e));
  if (e == NULL) {
    return -1;
  }
  e->key = strdup(key);
  e->value = strdup(value);
  if (e->key == NULL || e->value == NULL) {
    free(e->key);
    free(e->value);
    free(e);
    return -1;
  }
  e->next = t->buckets[i];
  t->buckets[i] = e;
  t->len++;
  return 0;
}

const char *kv_table_get(kv_table *t, const char *key) {
  uint64_t h = fnv1a(key);
  size_t i = h % t->cap;

  for (kv_entry *e = t->buckets[i]; e != NULL; e = e->next) {
    if (strcmp(e->key, key) == 0) {
      return e->value;
    }
  }
  return NULL;
}