// Todo: @IMPORTANT have a default hash function
// - use MeowHash? (for non-security usages)

// Todo: choose better hashmap indices (index = hash % x)
// - I remember that x should be smaller than cnt_indices,
//   to achieve a better distribution.

// Todo: ability to delete hashmap entries


#ifndef FS_HASHMAP_H
#define FS_HASHMAP_H



/*********************
 *     INTERFACE     *
 *********************/

#ifndef FS_HASHMAP_INTERFACE
#define FS_HASHMAP_INTERFACE

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef FS_HASHMAP_DEF
#define FS_HASHMAP_DEF static
#endif

#define FS_HASHMAP_COMPARE(name) bool name(void *key, void *entry_key)
typedef FS_HASHMAP_COMPARE(fs_hashmap_compare);

#define FS_HASHMAP_HASH(name) uint32_t name(void *key)
typedef FS_HASHMAP_HASH(fs_hashmap_hash);


typedef struct {
    uint32_t cnt_allocated;
    uint32_t cnt_active;
    uint32_t key_size;
    uint32_t value_size;
    fs_hashmap_hash *hash;
    fs_hashmap_compare *compare;
    void *buff;
} FS_Hashmap;


FS_HASHMAP_DEF void  fs_hashmap_init(FS_Hashmap *map, uint32_t cnt, uint32_t value_size, uint32_t key_size, fs_hashmap_hash *hash, fs_hashmap_compare *compare);
FS_HASHMAP_DEF void  fs_hashmap_add(FS_Hashmap *map, void *key, void *value);
FS_HASHMAP_DEF void* fs_hashmap_get(FS_Hashmap *map, void *key);
FS_HASHMAP_DEF void  fs_hashmap_destroy(FS_Hashmap *map);

#endif // FS_HASHMAP_INTERFACE




/**************************
 *     IMPLEMENTATION     *
 **************************/

#ifdef FS_HASHMAP_IMPLEMENTATION

FS_HASHMAP_DEF void fs_hashmap_init(FS_Hashmap *map, uint32_t cnt, uint32_t value_size, uint32_t key_size, fs_hashmap_hash *hash, fs_hashmap_compare *compare)
{
    uint64_t size = cnt * (value_size + key_size + sizeof(uint32_t));
    void *buff = malloc(size);
    if (!buff) return;


    map->cnt_allocated = buff ? cnt : 0;
    map->cnt_active = 0;
    map->key_size = key_size;
    map->value_size = value_size;
    map->hash = hash;
    map->compare = compare;
    map->buff = buff;
    memset(map->buff, 0, size);
}

FS_HASHMAP_DEF void fs_hashmap_add(FS_Hashmap *map, void *key, void *val)
{
    uint32_t key_size = map->key_size;
    uint32_t val_size = map->value_size;
    uint32_t cnt_allocated = map->cnt_allocated;
    uint32_t entry_size = sizeof(uint32_t) + key_size + val_size;
    void *buff = map->buff;

    uint32_t hash = map->hash(key);

    uint32_t index = hash % cnt_allocated;
    if (index == 0) index++;
    uint32_t collision_offset = 1;
    while (1)
    {
        void *entry = buff + index*entry_size;
        uint32_t entry_hash = *(uint32_t*)entry;
        if (!entry_hash) break;

        index += collision_offset;
        collision_offset++;
        while (index >= cnt_allocated) index -= cnt_allocated;
    }
    printf("index was %d\n", index);

    void *entry = buff + index*entry_size;
    uint32_t *entry_hash = entry;
    void *entry_key = entry + sizeof(hash);
    void *entry_val = entry + sizeof(hash) + key_size;
    memcpy(entry_hash, &hash, sizeof(hash));
    memcpy(entry_key, key, key_size);
    memcpy(entry_val, val, val_size);

    map->cnt_active++;
}

FS_HASHMAP_DEF void* fs_hashmap_get(FS_Hashmap *map, void *key)
{
    uint32_t key_size = map->key_size;
    uint32_t val_size = map->value_size;
    void *buff = map->buff;

    uint32_t hash = map->hash(key);
    uint32_t entry_size = sizeof(hash) + key_size + val_size;
    uint32_t index = hash;
    if (index == 0) index++;
    uint32_t collision_offset = 1;
    while (1)
    {
        void    *entry      = buff + index*entry_size;
        uint32_t entry_hash = *(uint32_t*)entry;
        void    *entry_key  = entry + sizeof(hash);
        void    *entry_val  = entry + sizeof(hash) + key_size;

        if (entry_hash == hash && map->compare(key, entry_key)) return entry_val;

        index += collision_offset;
        collision_offset++;
    }
}

FS_HASHMAP_DEF void fs_hashmap_destroy(FS_Hashmap *map)
{
    free(map->buff);
    map->cnt_allocated = 0;
}

#endif // FS_HASHMAP_IMPLEMENTATION
#endif // FS_HASHMAP_H
