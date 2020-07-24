#ifndef __DHT_TRIE__
#define __DHT_TRIE__
#include <stdio.h>

/**
 * struct representing single Huffman table
 */
typedef struct dhtTrie dhtTrie;

int showDebugInfo(dhtTrie*, char*, int);

/** 
 * Returns true if huffman table node has no value
 */
unsigned char isEmpty(dhtTrie*);

/**
 * Returns value stored at huffman table node. 
 * Assumes isEmpty() evaulates to False for this node.
 */
unsigned char getValue(dhtTrie*);

/**
 * Given a certain node in some huffman table, returns 
 * child node indicated by char parameter.
 *
 * Assumes char parameter is 0 or 1
 */
dhtTrie* traverseTrie(dhtTrie*, char);

#define MAX_NUMBER_OF_TABLES 8  // max number of Huffman tables in a JPG
#define MAX_NUMBER_OF_TABLES_ALLOWED 6  // We support only YCrCb
                                        // and 2 tables per color channel
/**
 * struct for storeing Huffman tables defined in the DHT segment(s)
 * index of a table in tables is 2*tableId + isDC
 */ 
typedef struct dhts {
    unsigned char tablesLeftToMake;
    dhtTrie *tables[MAX_NUMBER_OF_TABLES];
} dhts;


/**
 * Populates tables of dhts* with table that will be read from FILE*,
 * returning 1 if there was an issue and 0 otherwise
 * Assumes FILE*'s pointer is at start of a DHT segment
 */
int buildDhts(FILE*, dhts*);

/**
 * Frees memory allocated from heap by dhts* struct 
 */
void destroyDhts(dhts*);

void destroyDhtTrie(dhtTrie *t);

#endif