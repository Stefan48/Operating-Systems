#ifndef HASHMAP_H
#define HASHMAP_H

#define MAX_HASH 1000

#include <stdio.h>

typedef struct Hashmap
{
	char ***entries[MAX_HASH];
	int count[MAX_HASH];
	int size[MAX_HASH];
	
	
} Hashmap;

// computes hash of a string
int getHash(char *str);

// initializes hashmap
// returns 0 on success, error code otherwise
int initializeHashmap(Hashmap *hashmap);

// destroys hashmap (frees allocated memory)
void destroyHashmap(Hashmap *hashmap);

// returns the value associated with a key
// (or NULL if the key doesn't exist)
char *getValue(Hashmap *hashmap, char *key);

// inserts a key-value pair in the hashmap
// returns 0 on success, error code otherwise
int insertKeyValue(Hashmap *hashmap, char *key, char *value);

// removes a key-value pair from the hashmap
// returns 0 on success, error code otherwise
int removeKey(Hashmap *hashmap, char *key);

// prints all key-value pairs in hashmap
void printHashmap(Hashmap *hashmap, FILE *stream);


#endif // HASHMAP_H
