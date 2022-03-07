#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR -1
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)

typedef struct dict_entry {
    void *key;
    void *val;
    struct dict_entry *next;
} dict_entry;

typedef struct dict_type {
    uint64_t (*hash_func)(const void *key);
    void *(*key_dup)(void *private_data, const void *key);
    void *(*val_dup)(void *private_data, const void *obj);
    int (*key_compare)(void *private_data, const void *key1, const void *key2);
    void (*key_destructor)(void *private_data, void *key);
    void (*val_destructor)(void *private_data, void *obj);
} dict_type;

typedef struct dict {
    dict_entry **table;
    dict_type *type;
    unsigned long size;
    unsigned long sizemask;
    unsigned long used;
    void *private_data;
} dict;

typedef struct dict_iterator {
    dict *d;
    long index;
    dict_entry *entry, *next_entry;
} dict_iterator;

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE 4

/* ------------------------------- Macros ------------------------------------*/
#define dict_free_entry_val(ht, entry) \
    if ((ht)->type->val_destructor)    \
    (ht)->type->val_destructor((ht)->private_data, (entry)->val)

#define dict_set_entry_val(d, entry, _val_)                              \
    do {                                                                 \
        if ((d)->type->val_dup)                                          \
            (entry)->val = (d)->type->val_dup((d)->private_data, _val_); \
        else                                                             \
            (entry)->val = (_val_);                                      \
    } while (0)

#define dict_free_entry_key(d, entry) \
    if ((d)->type->key_destructor)    \
    (d)->type->key_destructor((d)->private_data, (entry)->key)

#define dict_set_entry_key(d, entry, _key_)                              \
    do {                                                                 \
        if ((d)->type->key_dup)                                          \
            (entry)->key = (d)->type->key_dup((d)->private_data, _key_); \
        else                                                             \
            (entry)->key = (_key_);                                      \
    } while (0)

#define dict_compare_keys(d, key1, key2)                         \
    (((d)->type->key_compare)                                    \
         ? (d)->type->key_compare((d)->private_data, key1, key2) \
         : (key1) == (key2))

#define dict_hash_key(d, key) (d)->type->hash_func(key)
#define dict_get_entry_key(he) ((he)->key)
#define dict_get_entry_val(he) ((he)->val)
#define dict_slots(ht) ((ht)->size)
#define dict_size(ht) ((ht)->used)

/* Generic hash function (a popular one from Bernstein).
 * I tested a few and this was the best. */
static unsigned int dictGenHashFunction(const unsigned char *buf, int len)
{
    unsigned int hash = 5381;
    while (len--)
        hash = ((hash << 5) + hash) + (*buf++); /* hash * 33 + c */
    return hash;
}

static inline void _dict_reset(dict *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

static inline void _dict_init(dict *ht, dict_type *type, void *private_data_ptr)
{
    _dict_reset(ht);
    ht->type = type;
    ht->private_data = private_data_ptr;
}

static inline dict *dict_create(dict_type *type, void *private_data_ptr)
{
    dict *ht = malloc(sizeof(*ht));
    if (!ht)
        return NULL;
    _dict_init(ht, type, private_data_ptr);
    return ht;
}
/* Our hash table capability is a power of two */
static inline unsigned long _dict_next_power(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;
    if (size >= LONG_MAX)
        return LONG_MAX + 1LU;
    /*make sure the new size is always greater than old size and being power of
     * two*/
    while (1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Expand or create the hash table */
static inline int dict_expand(dict *ht, unsigned long size)
{
    dict tmp_hashtable;
    unsigned long realsize = _dict_next_power(size);
    unsigned long i;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hashtable */
    if (ht->used > size)
        return DICT_ERR;

    _dict_init(&tmp_hashtable, ht->type, ht->private_data);

    tmp_hashtable.size = realsize;
    tmp_hashtable.sizemask = realsize - 1;
    tmp_hashtable.table = calloc(realsize, sizeof(dict_entry *));
    /* Copy all the elements from the old to the new table:
     * note that if the old hash table is empty ht->size is zero,
     * so dictExpand just creates an hash table. */
    tmp_hashtable.used = ht->used;
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dict_entry *he, *next_he;
        if (ht->table[i] == NULL)
            continue;
        /* For each hash entry on this slot... */
        he = ht->table[i];
        while (he) {
            unsigned int h;
            next_he = he->next;
            /* Get the new element index */
            h = dict_hash_key(ht, he->key) & tmp_hashtable.sizemask;
            he->next = tmp_hashtable.table[h];
            tmp_hashtable.table[h] = he;
            ht->used--;
            /*Pass to the next element*/
            he = next_he;
        }
    }
    assert(ht->used == 0);
    free(ht->table);
    /*Remap the new hashtable to the old one*/
    *ht = tmp_hashtable;

    return DICT_OK;
}
/* If the hash table is empty expand it to the initial size,
 * if the table is "full" dobule its size. */
static inline int _dict_expand_if_needed(dict *ht)
{
    if (ht->size == 0)
        return dict_expand(ht, DICT_HT_INITIAL_SIZE);
    if (ht->size == ht->used)
        return dict_expand(ht, ht->size * 2);
    return DICT_OK;
}

/* Returns the index of a free slot that can be populated with
 * an hash entry for the given 'key'.
 * If the key already exists, -1 is returned. */
static inline long _dict_key_index(dict *ht, const void *key)
{
    unsigned int hash_val;
    dict_entry *he;

    /* Expand the hashtable if needed */
    if (_dict_expand_if_needed(ht) == DICT_ERR)
        return -1;

    /*Compute the key hase value*/
    hash_val = dict_hash_key(ht, key) & ht->sizemask;
    /*search if this slot does not already contain the given key */
    he = ht->table[hash_val];

    while (he) {
        if (dict_compare_keys(ht, key, he->key))
            return -1;
        he = he->next;
    }
    return hash_val;
}

/*Add an element to the target hash table*/
static inline int dict_add(dict *ht, void *key, void *val)
{
    int index;
    dict_entry *entry;

    /* Get the index of the new element, or -1 if
     * the element already exists. */

    if ((index = _dict_key_index(ht, key)) == -1) {
        /*if (key != NULL)
            free(key);*/
        return DICT_ERR;
    }
    /* Allocates the memory and stores key */
    entry = malloc(sizeof(*entry));
    entry->next = ht->table[index];
    ht->table[index] = entry;

    /*set the hash entry field*/
    dict_set_entry_key(ht, entry, key);
    dict_set_entry_val(ht, entry, val);
    ht->used++;
    /*if (key != NULL)
        free(key);*/
    return DICT_OK;
}
static dict_entry *dict_find(dict *ht, const void *key)
{
    dict_entry *he;
    unsigned int h;

    if (ht->size == 0)
        return NULL;
    h = dict_hash_key(ht, key) & ht->sizemask;
    he = ht->table[h];
    while (he) {
        if (dict_compare_keys(ht, key, he->key))
            return he;
        he = he->next;
    }
    return NULL;
}
/* Add an element, discarding the old if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation. */
static int dict_replace(dict *ht, void *key, void *val)
{
    dict_entry *entry, aux_entry;
    /* Try to add the element. If the key
     * does not exists dictAdd will succeed. */
    if (dict_add(ht, key, val) == DICT_OK)
        return 1;
    /* It already exists, get the entry */
    entry = dict_find(ht, key);
    /* Free the old value and set the new one */
    /*設置新值並釋放舊值。
     *必須按照此順序進行，因為 value 可能完全
     *與上一個相同。在這種情況下，可以透過採取 reference counting 的作法
     */
    aux_entry = *entry;
    dict_set_entry_val(ht, entry, val);
    dict_free_entry_val(ht, &aux_entry);
    return 0;
}
static int dict_delete(dict *ht, const void *key)
{
    unsigned int hash_val;
    dict_entry *de, *prev_de;
    if (ht->size == 0)
        return DICT_ERR;
    hash_val = dict_hash_key(ht, key) & ht->sizemask;
    de = ht->table[hash_val];
    prev_de = NULL;
    while (de) {
        if (dict_compare_keys(ht, key, de->key)) {
            /*Unlink the element from the list*/
            if (prev_de)
                prev_de->next = de->next;
            else
                ht->table[hash_val] = de->next;
            dict_free_entry_key(ht, de);
            dict_free_entry_val(ht, de);
            free(de);
            ht->used--;
            return DICT_OK;
        }
        prev_de = de;
        de = de->next;
    }
    return DICT_ERR;
}
/* Destroy an entire hash table */
static int _dict_clear(dict *ht)
{
    unsigned long i;
    /*Free all the elements*/
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        dict_entry *he, *next_he;
        if ((he = ht->table[i]) == NULL)
            continue;
        while (he) {
            next_he = he->next;
            dict_free_entry_key(ht, he);
            dict_free_entry_val(ht, he);
            free(he);
            ht->used--;
            he = next_he;
        }
    }
    /*Free the table and the allocated structure*/
    free(ht->table);
    /*Re-initialize the table*/
    _dict_reset(ht);
    return DICT_OK; /*Actually,this never fail*/
}
/* Clear & Release the hash table */
static void dict_realease(dict *ht)
{
    _dict_clear(ht);
    free(ht);
}
#endif