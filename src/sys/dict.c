/**
 * @file sys/dict.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

VOID FspDictInitialize(FSP_DICT *Dict,
    FSP_DICT_EQUAL_FUNCTION *EqualFunction, FSP_DICT_HASH_FUNCTION *HashFunction,
    FSP_DICT_ENTRY **Buckets, ULONG BucketCount)
{
    RtlZeroMemory(Buckets, BucketCount * sizeof Buckets[0]);
    Dict->EqualFunction = EqualFunction;
    Dict->HashFunction = HashFunction;
    Dict->Buckets = Buckets;
    Dict->BucketCount = BucketCount;
}

FSP_DICT_ENTRY *FspDictGetEntry(FSP_DICT *Dict, PVOID Key)
{
    ULONG Index = Dict->HashFunction(Key) % Dict->BucketCount;
    for (FSP_DICT_ENTRY *Entry = Dict->Buckets[Index]; Entry; Entry = Entry->Next)
        if (Dict->EqualFunction(Key, Entry->Key))
            return Entry;
    return 0;
}

VOID FspDictSetEntry(FSP_DICT *Dict, FSP_DICT_ENTRY *Entry)
{
    ULONG Index = Dict->HashFunction(Entry->Key) % Dict->BucketCount;
    Entry->Next = Dict->Buckets[Index];
    Dict->Buckets[Index] = Entry;
}

FSP_DICT_ENTRY *FspDictRemoveEntry(FSP_DICT *Dict, PVOID Key)
{
    ULONG Index = Dict->HashFunction(Key) % Dict->BucketCount;
    for (FSP_DICT_ENTRY **PEntry = &Dict->Buckets[Index]; *PEntry; PEntry = &(*PEntry)->Next)
        if (Dict->EqualFunction(Key, (*PEntry)->Key))
        {
            FSP_DICT_ENTRY *Entry = *PEntry;
            *PEntry = Entry->Next;
            return Entry;
        }
    return 0;
}
