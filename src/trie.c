#include <stdlib.h>
#include "trie.h"
#include "csteg.h"
#include "queue.h"

#define MAX_NUMBER_OF_TABLES_ALLOWED 6  // 2 tables per color channel

/*
 *  !isEmpty && one == 0 && zero == 0 <--> value is a value and corresponds to 
 *      encoding of the huffman table.
 *   isEmpty && one == 0 && zero == 0 <--> node should be removed from table.
 */

struct dhtTrie { 
    unsigned char value;           // value obtained by going down DHT trie strucutre if isEmpty is false
    unsigned char isEmpty;         // if 0 this is a leaf node.
    struct dhtTrie *one;           // node obtained by going through one branch
    struct dhtTrie *zero;          // node obtained by going through zero branch
};

/**
 * Frees memory allocated in heap to dhtTrie struct
 */
void destroyDhtTrie(dhtTrie *t) {
    if(t == NULL) {
        return;
    }

    destroyDhtTrie(t->one);
    destroyDhtTrie(t->zero);
    free(t);
}

/*TODO: try to do this and add to create fuction*/
void pruneDhtTrie(dhtTrie* root) {
    if (root == NULL) {
        return;
    }

    if (!root->isEmpty) {
        destroyDhtTrie(root->zero);
        root->zero = NULL;
        destroyDhtTrie(root->one);
        root->one = NULL;
    } else {
        pruneDhtTrie(root->zero);
        pruneDhtTrie(root->one);
    }

}

void destroyDhts(dhts* d) {
    if(d == NULL) {
        return;
    }

    for (int i = 0; i < MAX_NUMBER_OF_TABLES; i++) {
        destroyDhtTrie(d->tables[i]);
    }
    free(d);
}

/**
 * Creates a dhtTrie structure, returning its pointer
 */ 
dhtTrie* initNode() {
    dhtTrie *root = (dhtTrie*)malloc(sizeof(dhtTrie));
    root->isEmpty = 1;
    root->zero = NULL;
    root->one = NULL;
    return root;
}

/**
 * Constructs a single Huffman table from jpegFile corresponding to the data 
 * jpegFile's curssor is pointing to. Every byte read from jpegFile corresponds
 * to an increase of *bytesProcessed
 * 
 * Assumes that jpegFile cursor is positioned right at the first of the 16 bytes
 * indicated the number of symbols per bit length
 */
dhtTrie* createDhtTrie(FILE* jpegFile, unsigned short *bytesProcessed) {
    unsigned char elementsPerDepth[16]; // [i] == number of values 
                                        // encoded with i+1 bits.
    fread(elementsPerDepth, 1, 16, jpegFile);
    *bytesProcessed += 16;
    // Initialize array of values/symbols
    size_t dataLength = 0;
    for (int i = 0; i < 16; i++) {
        dataLength += elementsPerDepth[i];
    }
    char* data = malloc(dataLength);
    fread(data, 1, dataLength, jpegFile);
    *bytesProcessed += dataLength;

    int indexOfData = 0; // number of elements of data read so far
    // Use remaining data to construct trie structure
    dhtTrie *root = initNode();
    root->zero = initNode();
    root->one = initNode();
    // init queue of nodes to place new elements into or expand out
    queue_t *nodeQueue = queue_new();
    queue_append(nodeQueue, (void*)root->zero);
    queue_append(nodeQueue, (void*)root->one);
    /*
     Constructs dhTrie structure for the given data
     Iteration Invariant(s)
        Number of chars to add per bucket is less-than or equal to number of 
        elements in nodeQueue
        After every iteration, nodeQueue will only contain nodes in the same depth 
        in left-right order
    */
    for(char bucket = 0; bucket < 16; bucket++) {
        dhtTrie* tempNode;
        // Place list of values into appropriate nodes
        for(char i = 0; i < elementsPerDepth[bucket]; i++) {
            if (queue_dequeue(nodeQueue, (void**)&tempNode) != 0) { // failed
                // TODO: Add more stuff here, like freeing stuff
                destroyDhtTrie(root);
                queue_free(nodeQueue);
                free(data);
                return NULL;
            }

            tempNode->value = data[indexOfData];
            tempNode->isEmpty = 0;
            indexOfData++;
        }

         // Expand nodes at current depth that don't have a value
        int nodesToExpand = queue_length(nodeQueue);
        for(int i = 0; i < nodesToExpand; i++) {
            queue_dequeue(nodeQueue, (void**)&tempNode);
            tempNode->zero = initNode();
            queue_append(nodeQueue, (void*)tempNode->zero);
            tempNode->one = initNode();
            queue_append(nodeQueue, (void*)tempNode->one);
        }
    }
    
    // Free queue since no longer needed
    void* dumpBuffer = NULL;
    while (queue_length(nodeQueue) > 0) {
        queue_dequeue(nodeQueue, &dumpBuffer);
    }
    queue_free(nodeQueue);

    free(data);
    
    // Remove unneeded nodes from tree
    pruneDhtTrie(root); // Get rid of unneeded nodes
    return root;
}

/**
 * Returns length of DHT segment, or 0 if jpegFile cursor is not located right
 * in front of the start of a DHT segment when this function is called
 */
unsigned short getLengthOfDHTSegment(FILE* jpegFile) {
    unsigned short buffer[3];
    fread(buffer, 2, 2, jpegFile);
    buffer[0] = BYTE_TO_SHORT_VALUE(buffer[0]);
    
   if (buffer[0] != DHT_START) {
        printf("Rejected %#06x  is not %#06x\n", buffer[0], DHT_START);
        return 0;  // length of payload must be >= 2
    } else {
        return BYTE_TO_SHORT_VALUE(buffer[1]);
    }
}

/**
 * Recursively traverse dhTrie structure and print its value-key pairs. 
 */
int showDebugInfo(dhtTrie* t, char* path, int depth) {
    if(t == NULL) {
        return 1;
    }
    if (!t->isEmpty) {
        char buffer[100];
        for(int i = 0; i < depth; i++) {
            buffer[i] = path[i];
        }
        buffer[depth] = 0;
        printf("Path: %16s, Value: %4d   Depth %2d\n", buffer, t->value, depth);
        return 0;
    }

    path[depth] = '0';
    int resultLeft = showDebugInfo(t->zero, path, depth + 1);
    path[depth] = '1';
    int resultRight = showDebugInfo(t->one, path, depth + 1);
    
    int result = resultLeft + resultRight;
    if(depth == 0) {
        printf("\n\n");
    }
    return result;
}

/**
 * Moves data past dhts portions
 */
dhts* createDhts(FILE *jpegFile) {
    #ifdef TESTING
        printf("\tDHT Data Parseing:\n");
    #endif

    dhts *tables = (dhts*)calloc(1, sizeof(dhts)); // Initialize so that all pointers are NULL
    tables->tablesLeftToMake = MAX_NUMBER_OF_TABLES_ALLOWED;

    // Make sure this is a DHT segment and get length of segment
    unsigned short segmentLength = getLengthOfDHTSegment(jpegFile);
    if (segmentLength == 0) {
        destroyDhts(tables);
        return NULL;
    }

    unsigned short bytesProcessed = 2;  // segmentLength includes length bytes
    while (bytesProcessed < segmentLength) {
        if (tables->tablesLeftToMake == 0) {
            printf("TOO MANY TABLES");
            destroyDhts(tables);
            return NULL;
        }
        // Get index to place table in tables->tables
        unsigned char indexData;
        fread(&indexData, 1, 1, jpegFile);
        bytesProcessed++;
        int discreteOrAlternating = indexData >> 4;
        int tableId = indexData & 15; // 15 = 0b1111

        #ifdef TESTING
            printf("\tTABLE: byte=%u id=%u isAC=%u\n", 
                    indexData, tableId, discreteOrAlternating);
        #endif
        int index = 2*tableId + discreteOrAlternating;
        if (index < 0 || index > MAX_NUMBER_OF_TABLES || 
            tables->tables[index] != NULL) {
            printf("ERROR: Invalid Huffman Table id values: %d %d\n",
                    tableId, discreteOrAlternating);
            destroyDhts(tables);
            return NULL;
        }
        
        dhtTrie *tempNode = createDhtTrie(jpegFile, &bytesProcessed);
        if(!tempNode) {
            destroyDhts(tables);
            return NULL;
         } else {
             tables->tables[index] = tempNode;
             tables->tablesLeftToMake--;
         }
    }

    // Print out whole table if testing
    #ifdef TESTING
        for(int i = 0; i < MAX_NUMBER_OF_TABLES; i++) {
            if (tables->tables[i] != NULL) {
                printf("TABLE %d\n", i);
                char path[100];
                for(int j = 0; j < 100; j++) {
                    path[j] = 0;
                }
                showDebugInfo(tables->tables[i], path, 0);
            }
        }
    #endif

    return tables;
}


/* FUNCTIONS EXPOSING ATTRIBUTES OF dhtTrie struct */

/**
 * Returns 0 if t isEmpty and 1 if it is not (it has a valid value and is a leaf node)
 * Assumes t is a non-null dhtTrie object
 */
unsigned char isEmpty(dhtTrie* t) {
    return t->isEmpty;
}

/**
 * Get the value stored in t
 * Assumes t is a non-null dhtTrie object
 */
unsigned char getValue(dhtTrie* t) {
    return t->value;
}

/**
 * Returns NULL if t is null or bit is not 0 or 1,
 * else returns the child of t dictated by bit
 */ 
dhtTrie* traverseTrie(dhtTrie* t, char bit) {
    if(t == NULL || (unsigned int) bit > 1) {
        return NULL;
    }
    return bit == 0 ? t->zero : t->one;
}
