#ifndef KV_HASH_H
#define KV_HASH_H

#include <stddef.h>
#include <stdint.h>

/* FNV-1a 64-bit hash of a null-terminated string. */
uint64_t fnv1a(const char *s);

/* A single entry in a bucket's chain */
typedef struct kv_entry {
  char *key;
  char *value;
  struct kv_entry *next;
} kv_entry;

/* The hash table */
typedef struct kv_table {
  kv_entry **buckets; /* array of `cap` pointers, each to a chain head */
  size_t cap;         /* number of buckets*/
  size_t len;         /* number of entries currently stored*/
} kv_table;

/* Allocate a new table with `cap` buckets. Returns NULL on allocation failure */
kv_table *kv_table_new(size_t cap);

/* Free the table and all entries it owns. Safe to call with NULL */
void kv_table_free(kv_table *t);

#endif