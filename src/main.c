#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_get(kv_table *t, const char *key, const char *expected) {
  const char *got = kv_table_get(t, key);
  if (expected == NULL) {
    if (got == NULL) {
      printf("  GET %-10s -> (nil)  [ok]\n", key);
    } else {
      printf("  GET %-10s -> \"%s\"  [FAIL: expected nil]\n", key, got);
    }
  } else {
    if (got != NULL && strcmp(got, expected) == 0) {
      printf("  GET %-10s -> \"%s\"  [ok]\n", key, got);
    } else {
      printf("  GET %-10s -> %s  [FAIL: expected \"%s\"]\n", key,
             got ? got : "(nil)", expected);
    }
  }
}

int main(void) {
  /* Create a small table */
  kv_table *t = kv_table_new(16);
  if (t == NULL) {
    fprintf(stderr, "failed to allocate table\n");
    return 1;
  }

  printf("inserting foo=bar, hello=world, name=logan\n");
  kv_table_set(t, "foo", "bar");
  kv_table_set(t, "hello", "world");
  kv_table_set(t, "name", "logan");
  printf("len=%zu cap=%zu\n", t->len, t->cap);

  printf("\nlookups:\n");
  check_get(t, "foo", "bar");
  check_get(t, "hello", "world");
  check_get(t, "name", "logan");
  check_get(t, "missing", NULL);

  printf("\nupdate foo=baz\n");
  kv_table_set(t, "foo", "baz");
  printf("len=%zu (should still be 3)\n", t->len);
  check_get(t, "foo", "baz");

  printf("\ninsert many keys to force chain growth\n");
  char key[16], val[16];
  for (int i = 0; i < 20; i++) {
    snprintf(key, sizeof(key), "k%d", i);
    snprintf(val, sizeof(val), "v%d", i);
    kv_table_set(t, key, val);
  }
  printf("len=%zu cap=%zu (load factor = %.2f)\n", t->len, t->cap,
         (double)t->len / t->cap);

  check_get(t, "k0", "v0");
  check_get(t, "k7", "v7");
  check_get(t, "k19", "v19");

  kv_table_free(t);
  printf("\nfreed cleanly\n");

  return 0;
}