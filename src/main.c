#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

int main(void) {
    /* Create a small table */
    kv_table *t = kv_table_new(16);
    if (t == NULL) {
        fprintf(stderr, "failed to allocate table\n");
        return 1;
    }

    /* DEBUG STATEMENT */
    printf("table: cap=%zu len=%zu\n", t->cap, t->len);
    
    for ( size_t i=0; i < t->cap; i++) {
        if (t->buckets[i] != NULL) {
            fprintf(stderr, "bucket %zu was not NULL!\n", i);
            kv_table_free(t);
            return 1;
        }
    }

    printf("all %zu buckets are NULL as expected\n", t->cap);

    kv_table_free(t);

    /* Should be safe to call with NULL. */
    kv_table_free(NULL);
    printf("freed cleanly, including NULL\n");

    return 0;
}