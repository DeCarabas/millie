#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct SymbolEntry {
    struct MString *key;
    Symbol value;
};

#pragma GCC diagnostic pop

struct SymbolTable {
    uint32_t *hashes;
    struct SymbolEntry *entries;
    uint32_t capacity;
    uint32_t item_count;
    uint32_t resize_threshold;
    uint32_t mask;
};

static uint32_t _HashString(struct MString *string)
{
    uint32_t hash = MStringHash32(string);
    hash &= 0x7FFFFFFF; // Clear tombstone bit.
    if (hash == 0) { hash = 1; } // Never return empty.
    return hash;
}

static bool _IsDeleted(uint32_t hash)
{
    return (hash & 0x80000000) != 0;
}

static uint32_t _DesiredPosition(struct SymbolTable *table, uint32_t hash)
{
    return (hash & table->mask);
}

static uint32_t _ProbeDistance(struct SymbolTable *table, uint32_t hash,
                               uint32_t slot_index)
{
    uint32_t dist = slot_index + table->capacity;
    dist -= _DesiredPosition(table, hash);
    return dist & table->mask;
}

static void _Allocate(struct SymbolTable *table)
{
    table->entries = calloc(table->capacity, sizeof(struct SymbolEntry));
    table->hashes = calloc(table->capacity, sizeof(uint32_t));
    table->resize_threshold = (table->capacity * 90) / 100; // 90% load factor
    table->mask = table->capacity - 1;
}

static void _SetElement(struct SymbolTable *table, uint32_t pos, uint32_t hash,
                        struct MString *key, Symbol value)
{
    table->hashes[pos] = hash;
    table->entries[pos].key = key;
    table->entries[pos].value = value;
}

static void _InsertHelper(struct SymbolTable *table, uint32_t hash,
                          struct MString *key, Symbol value)
{
    uint32_t pos = _DesiredPosition(table, hash);
    uint32_t dist = 0;
    for(;;) {
        uint32_t existing_hash = table->hashes[pos];
        if (existing_hash == 0) {
            _SetElement(table, pos, hash, key, value);
            return;
        }

        uint32_t existing_probe_dist;
        existing_probe_dist = _ProbeDistance(table, existing_hash, pos);
        if (existing_probe_dist < dist) {
            if (_IsDeleted(existing_hash)) {
                _SetElement(table, pos, hash, key, value);
                return;
            }

            struct SymbolEntry t = table->entries[pos];
            _SetElement(table, pos, hash, key, value);
            key = t.key;
            value = t.value;
            hash = existing_hash;
            dist = existing_probe_dist;
        }

        pos = (pos + 1) & table->mask;
        dist += 1;
    }
}

static void _Grow(struct SymbolTable *table)
{
    struct SymbolEntry *old_entries = table->entries;
    uint32_t *old_hashes = table->hashes;
    uint32_t old_capacity = table->capacity;

    table->capacity *= 2;
    _Allocate(table);

    for(uint32_t i = 0; i < old_capacity; i++) {
        uint32_t hash = old_hashes[i];
        if (hash != 0 && !_IsDeleted(hash)) {
            _InsertHelper(
                table,
                hash,
                old_entries[i].key,
                old_entries[i].value
            );
        }
    }

    free(old_entries);
    free(old_hashes);
}

static Symbol _Find(struct SymbolTable *table, struct MString *key)
{
    uint32_t hash = _HashString(key);
    uint32_t pos = _DesiredPosition(table, hash);
    uint32_t dist = 0;
    for (;;) {
        uint32_t existing_hash = table->hashes[pos];
        if (table->hashes[pos] == 0) {
            return INVALID_SYMBOL;
        }
        if (dist > _ProbeDistance(table, existing_hash, pos)) {
            return INVALID_SYMBOL;
        }
        if (hash == existing_hash &&
            MStringEquals(key, table->entries[pos].key)) {
            return table->entries[pos].value;
        }
        pos = (pos + 1) & table->mask;
        dist += 1;
    }
}

struct SymbolTable *SymbolTableCreate()
{
    struct SymbolTable *result = calloc(1, sizeof(struct SymbolTable));
    result->capacity = 256;
    _Allocate(result);
    return result;
}

void SymbolTableFree(struct SymbolTable **table_ptr)
{
    struct SymbolTable *table = *table_ptr;
    *table_ptr = NULL;

    if (!table) { return; }
    for(uint32_t i = 0; i < table->capacity; i++) {
        MStringFree(&table->entries[i].key);
    }
    free(table->entries);
    free(table->hashes);
    free(table);
}

Symbol FindOrCreateSymbol(struct SymbolTable *table, struct MString *key)
{
    // See if we can find the symbol in the table first...
    Symbol symbol = _Find(table, key);
    if (symbol != INVALID_SYMBOL) { return symbol; }

    table->item_count += 1;
    symbol = (Symbol)(table->item_count);
    if (table->item_count >= table->resize_threshold) {
        _Grow(table);
    }

    key = MStringCopy(key);
    uint32_t hash = _HashString(key);
    _InsertHelper(table, hash, key, symbol);

    return symbol;
}
