#ifndef __C_STEGANOGRAPHY__
#define __C_STEGANOGRAPHY__

#include "trie.h"
/* 
 * struct for holding data for jpeg
 * 
 * Takes advantage that Color ids of Y, Cr, and Cb are 1, 2, and 3 respectively
 */
typedef struct jpegStats {
    unsigned short restartInterval;  // number of MCUs before a restart token
                                     // is encountered
    unsigned int mcuCount;           // number of MCUs in file
    unsigned short colorCounts[3];   // color_id - 1 ---> number of color 
                                     //values with that ID in MCU
    unsigned int totalColorCounts;  // sum of all elements in colorCounts
    // color_id - 1 ---> corresponding DC and AC tables
    dhtTrie* dcHuffmanTables[3];
    dhtTrie* acHuffmanTables[3];
    
} jpegStats;

#define JPEG_START 0xFFD8       // Starting two bytes of JPEG file
#define DQT_START  0xFFDB
#define START_OF_FRAME_0    0xFFC0
#define DRI_MARKER          0xFFDD
#define DHT_START  0xFFC4
#define JPEG_SOS   0xFFDA         // Starting two bytes of JPEG IMG data
#define JPEG_END   0xFFD9         // Signals the end of JPEG file

#define EOB 0 // End of line value, says to stop processing DC or AC of MCU
#define ZRL 0xF0 // Signifies 16 0-value coeficients
#define MARKER_LENGTH 2  // length of the marker of a JPEG segment

// Constants for ids of specific color values:
#define Y_ID 1   // luminance
#define CB_ID 2  // blue chrominance
#define CR_ID 3  // red chrominance
#define INVALID_COLOR(x) x < Y_ID || x > CR_ID // Determine if color supported
#define STD_MCU_SIZE 64 // 8x8
#define MAX_AC_COEFFICIENTS 63

// Macros to help with operations in imgMessagePrueba.c
/*Converting bytes read into short to original value from file*/
#define BYTE_TO_SHORT_VALUE(x) ((unsigned short)(x >> 8 | x << 8))

#define GET_FIRST_4_BITS(x) (x & 15)
#define GET_4_MSBs(x) (x >> 4)


#define MAX_MESSAGE_LENGTH_PLUS_1 1001 // max length of user's message.

// Bit masks to use for extracting particular bits of message char
#define FOURTH_2_BITS 3
#define THIRD_2_BITS 12
#define SECOND_2_BITS 48
#define FIRST_2_BITS 192
#define NUMBER_OF_BITMASKS 4
#define BITS_PER_MASK 2

#endif
