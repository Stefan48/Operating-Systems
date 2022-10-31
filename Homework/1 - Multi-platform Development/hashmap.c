#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

int getHash(char *str)
{	
    unsigned long hash = 5381;
    int c;
    while(1)
    {
    	c = *str;
    	if (!c)
    	{
    		break;
    	}
    	str++;
    	// hash = hash * 33 + c
    	hash = ((hash << 5) + hash) + c;
    }
	hash %= MAX_HASH;
    return hash;
}

// returns 0 on success, error code otherwise
int initializeHashmap(Hashmap *hashmap)
{
	int hash;
	int Error = 0;
	for(hash = 0; hash < MAX_HASH; ++hash)
	{
		hashmap->count[hash] = 0;
		if(!Error)
		{
			hashmap->entries[hash] = malloc(1 * sizeof(char**));
			if(hashmap->entries[hash] == NULL)
			{
				Error = 12;
				hashmap->size[hash] = 0;
			}
			else
			{
				// initially size = 1
				hashmap->size[hash] = 1;
			}
		}
		else
		{
			hashmap->size[hash] = 0;
		}
	}
	return Error;
}

void destroyHashmap(Hashmap *hashmap)
{
	int i, hash;
	for(hash = 0; hash < MAX_HASH; ++hash)
	{
		if(hashmap->entries[hash] != NULL)
		{
			for(i = 0; i < hashmap->count[hash]; ++i)
			{
				if(hashmap->entries[hash][i] != NULL)
				{
					if(hashmap->entries[hash][i][0] != NULL)
					{
						free(hashmap->entries[hash][i][0]);
					}
					if(hashmap->entries[hash][i][1] != NULL)
					{
						free(hashmap->entries[hash][i][1]);
					}
					free(hashmap->entries[hash][i]);
				}
			}
			free(hashmap->entries[hash]);
		}
	}
}

char *getValue(Hashmap *hashmap, char *key)
{
	int hash = getHash(key);
	int cnt = hashmap->count[hash];
	int i;
	for(i = 0; i < cnt; ++i)
	{
		if(strcmp(hashmap->entries[hash][i][0], key) == 0)
		{
			return hashmap->entries[hash][i][1];
		}
	}
	return NULL;
}

// returns 0 on success, error code otherwise
int insertKeyValue(Hashmap *hashmap, char *key, char *value)
{
	int hash = getHash(key);
	int len_key = strlen(key), len_value = strlen(value);
	int cnt = hashmap->count[hash];
	char *k, *v;
	if(getValue(hashmap, key) != NULL)
	{
		return 0;
	}
	if(cnt == hashmap->size[hash])
	{
		// reallocate
		hashmap->size[hash] <<= 1;
		hashmap->entries[hash] = realloc(hashmap->entries[hash], hashmap->size[hash] * sizeof(char**));
		if(hashmap->entries[hash] == NULL)
		{
			return 12;
		}
	}
	k = malloc((len_key + 1) * sizeof(char));
	if(k == NULL)
	{
		return 12;
	}
	strcpy(k, key);
	v = malloc((len_value + 1) * sizeof(char));
	if(v == NULL)
	{
		return 12;
	}
	strcpy(v, value);
	hashmap->entries[hash][cnt] = malloc(2 * sizeof(char*));
	if(hashmap->entries[hash][cnt] == NULL)
	{
		return 12;
	}
	hashmap->entries[hash][cnt][0] = k;
	hashmap->entries[hash][cnt][1] = v;
	hashmap->count[hash]++;
	return 0;
}

// returns 0 on success, error code otherwise
int removeKey(Hashmap *hashmap, char *key)
{
	int hash = getHash(key);
	int cnt = hashmap->count[hash];
	int i, j;
	for(i = 0; i < cnt; ++i)
	{
		if(strcmp(hashmap->entries[hash][i][0], key) == 0)
		{
			free(hashmap->entries[hash][i][0]);
			free(hashmap->entries[hash][i][1]);
			for(j = i; j < hashmap->count[hash] - 1; ++j)
			{
				hashmap->entries[hash][j][0] = hashmap->entries[hash][j+1][0];
				hashmap->entries[hash][j][1] = hashmap->entries[hash][j+1][1];
			}
			hashmap->count[hash]--;
			cnt--;
			free(hashmap->entries[hash][cnt]);
			break;
		}
	}
	if(cnt > 0 && (cnt << 1) < hashmap->size[hash])
	{
		// reallocate
		hashmap->size[hash] >>= 1;
		hashmap->entries[hash] = realloc(hashmap->entries[hash], hashmap->size[hash] * sizeof(char**));
		if(hashmap->entries[hash] == NULL)
		{
			return 12;
		}
	}
	return 0;
}

void printHashmap(Hashmap *hashmap, FILE *stream)
{
	int hash, i;
	for(hash = 0; hash < MAX_HASH; ++hash)
	{
		for(i = 0; i < hashmap->count[hash]; ++i)
		{
			fprintf(stream, "(%s, %s)\n", hashmap->entries[hash][i][0], hashmap->entries[hash][i][1]);
		}
	}
}
