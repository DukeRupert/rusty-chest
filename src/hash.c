#include "hash.h"
#include <stdlib.h>

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

  /* No entries to free yet — chains are all empty in Step 1.3. */
  free(t->buckets);
  free(t);
}