#include <stdlib.h>
#include <string.h>
#include "scanWorker.h"
#ifdef TESTING
    #include <assert.h>
#endif

// TODO: Consider restart interval and max # of mcus read

typedef struct mcu {
    unsigned char acCurrentlyOn; // stores coeficient currently at, in [0, 62]
    unsigned long index; // byte in buffer storing last bit of current 
                                    // AC of MCU
    unsigned char bit; // points to last bit of AC of MCU as index of 
                       // bufffer[index] 
                                    // as index in indexOfComponent
    unsigned char bitLength;  // stores number of bits AC referenced
                              // by mcu takes. in [1,F] 
} mcu;

// TODO: use totalSize somewhere
typedef struct scanWorker {
    unsigned char* scanBuffer;  // Stores all data after SOS segment
    unsigned long totalSize;  // Space alloced for scanBuffer;
                              // there should be totalSize bytes to write to jpg
    unsigned int mcusRead;  // Number of MCUs read  by scanWorker
    
    unsigned long bytesRead; // Number of bytes from scanBuffer read
    unsigned char bitCursor; // number of bits read in scanBuffer[bytesRead]
    unsigned char onSecondChrominance; // True if on Cr, False if on Cb 
    mcu* mcu;  // data pertaining to current MCU we are looking at
} scanWorker;

/**
 * Frees memory allocated for an mcu struct
 */ 
void destroyMCU(mcu *x) {
    free(x);
}

/**
 * Creates an MCU struct, returning its address if mcu created
 * successfully and NULL otherwise. DOES NOT INITIALISE ANY VALUES
 * 
 * Assumes stats->colorCounts has length 3
 */
mcu* initMCU(jpegStats *stats) {
    mcu* x = malloc(sizeof(mcu));
    if (x == NULL)
        return NULL;
    return x;
}

/**
 * Returns 1 if index corresponds to a position afer the scan of the image and
 * 0 otherwise
 * If index is > than scanBuffer size, returns true
 */
char isEndOfScan(scanWorker *sw, unsigned long index) {
    if (index >= sw->totalSize) {
        return 1;
    }
    unsigned short* end = (unsigned short*)&sw->scanBuffer[index];
    unsigned short realEnd = BYTE_TO_SHORT_VALUE(*end);
    return realEnd == JPEG_END;

}

/**
 * Reads the next bit from sw's scanBuffer and returns it after propperly 
 * incrementing sw's byte and bit cursors accordingly, unless current byte is
 * 
 * 
 * Skips past stuff-bytes, defined in function
 */
unsigned char nextBit(scanWorker* sw) {
    // Don't read if you are at end of scan
    if (isEndOfScan(sw, sw->bytesRead)) {
        return END_OF_FILE_ENCOUNTERED;
    }

    // Read next bit from scanBuffer and store in data
    unsigned char shiftValue = 7 - sw->bitCursor;
    unsigned char data = sw->scanBuffer[sw->bytesRead] & (1 << shiftValue);
    data = data >> shiftValue;
    
    sw->bitCursor++;  // increment bitCursor since one bit has been read
    #ifdef TESTING
        assert(data == 0 || data == 1);
        assert(sw->bitCursor <= 8);
    #endif
    // Advance byte cursor if a full byte has just been read
    if (sw->bitCursor >= 8) {
        sw->bitCursor = 0;
        sw->bytesRead++;
        // Skip useless 00 stuff-byte of FF00
        if (sw->bytesRead > 0 && sw->scanBuffer[sw->bytesRead] == 0 &&
            sw->scanBuffer[sw->bytesRead - 1] == 0xFF) {
            sw->bytesRead++;
        }
    }
    return data;
}

/**
 * Reads a coeficient of somce MCU and stores relevant data in the designated
 * locations. Sets *indexStorage and *bitStorage (later gurananteed) 
 * to EOB_ENCOUNTERED (> 8) if EOB encountered. *lengthStorage is also set to 0
 * in this case. Does not modify acCurrentlyOn
 * 
 * Assumes that scanner is at the start of a new coeficient (at first bit of its
 * length/0-count) and that mcuData->acCurrentlyOn accurately refflects the
 * AC of the current MCU we are currently working with.
 * 
 * Assumes nextBit does not read padding 0 bytes e.g. FF[00]
 */
int readComponentElement(scanWorker *scanner, mcu *mcuData,
                         dhtTrie *table, unsigned char isAc) {
    #ifdef TESTING
        assert(isAc <= 1);
        assert(table != NULL);
    #endif

    unsigned char coeficientsRead = 0;
    // Read length part bit by bit
    while (isEmpty(table)) {
        char bit = nextBit(scanner);
        if (bit == END_OF_FILE_ENCOUNTERED)
            return 1;
        table = traverseTrie(table, bit);
        if (table == NULL) {
            puts("ERROR: NULL TABLE");
            return 1;
        }
    }
    
    unsigned char numBits = getValue(table); // length of coeficient in bits
    mcuData->index = scanner->bytesRead; // Note: this not needed
    if (numBits == EOB) { 
        // No more coeficients to reads, realy only consequential for ACs
        mcuData->bit = EOB_ENCOUNTERED;
        mcuData->bitLength = 0;
        coeficientsRead = isAc ? MAX_AC_COEFFICIENTS - mcuData->acCurrentlyOn : 
                          1 ;

    } else if(numBits == ZRL) { // 16 0s processed
        #ifdef TESTING
            assert(isAc);
        #endif
        mcuData->bit = ZRL_ENCOUNTERED;
        mcuData->bitLength = 0;
        coeficientsRead = 16;
    } else {
        if (isAc) { 
            // Make numBits actual length of coeficient and set coeficientsRead
            // to number of 0s, actual coeficient counted at end
            coeficientsRead = GET_4_MSBs(numBits);
            numBits = GET_FIRST_4_BITS(numBits);
            #ifdef TESTING
                //printf("\tNUMBER OF 0s: %d\n", *coeficientsRead);
            #endif

        } 
        mcuData->bitLength = numBits; // store coeficient bit-length data

        // Get scannner's cursor's pointing to last bit of current coeficient
        // and store position in imcuData. Then advance to next component
        #ifdef TESTING
            //puts("READING VALUE:");
        #endif
        for (int i = 0; i < numBits - 1; i++) {
            // Just skip through all but last bit of current coeficinet
            if (nextBit(scanner) == END_OF_FILE_ENCOUNTERED)
                return 1;
        }
        mcuData->index = scanner->bytesRead;
        mcuData->bit = scanner->bitCursor;

        if (nextBit(scanner) == END_OF_FILE_ENCOUNTERED) { // get past last bit of value
            return 1;
        }
        coeficientsRead++;
    }

    mcuData->acCurrentlyOn += coeficientsRead;
    return 0;
}

/**
 * Checks to see that stats->restartInterval MCUs have been read and, if so,
 * reads past the restart interval and any useless bits in between the
 * interval marker and the bit referenced by scanner's attributes. Returns 1
 * in the case of error (or when isEndOfScan(scanner, scanner->bytesRead)) is
 * true.
 *
 * If this is not the case, function is a noop
 */
int skipPastRestartInterval(scanWorker *scanner, jpegStats *stats) {
    if (stats->restartInterval != 0 &&
        scanner->mcusRead % stats->restartInterval == 0) {
        // Skip past useless bits
        while (scanner->bitCursor != 0) {
            unsigned char bitRead = nextBit(scanner);
            if (bitRead == END_OF_FILE_ENCOUNTERED) {
                return 1;
            }
            #ifdef TESTING
                assert(bitRead == 1);
            #endif
        }
        // Make sure not at EOS
        if (isEndOfScan(scanner, scanner->bytesRead)) {
            return 1;
        }
        #ifdef TESTING
            printf("RESTART ENCOUNTERED: %hX %hX ||%hX\n",
                    scanner->scanBuffer[scanner->bytesRead],
                    scanner->scanBuffer[scanner->bytesRead + 1],
                    scanner->scanBuffer[scanner->bytesRead + 2] );
            assert(scanner->scanBuffer[scanner->bytesRead] == 0xFF);
            unsigned char p2 = scanner->scanBuffer[scanner->bytesRead + 1];
            assert(p2 >= 0xD0 && p2 <= 0xD7);
        #endif
        scanner->bytesRead += 2;
    }
    return 0;
}

/**
 * Using jpegStats and scanner, read current MCU, populating the two tables
 * of mcuData. Returns 0 upon success and 1 otherwise. mcuData is populated with
 * the current AC coeficient we are currently on, regardless of writeability
 * 
 * Increments mcus read by 1
 * 
 * Assumes restart interval >= 2
 */
int loadNextMCU(mcu* mcuData, scanWorker* scanner, jpegStats* stats) {
    scanner->mcusRead++;  // increment number of mcus processed
    if (skipPastRestartInterval(scanner, stats)) {
        return 1;
    }
    scanner->onSecondChrominance = 0;

    // Skip past Y-components
    int colorId = Y_ID - 1;
    dhtTrie *dcTable = stats->dcHuffmanTables[colorId];
    dhtTrie *acTable = stats->acHuffmanTables[colorId];
    mcu mcuBuffer; // stores useless data
    for (int comp = 0; comp < stats->colorCounts[colorId]; comp++) {
         mcuBuffer.acCurrentlyOn = 0;  // misuesed for first part for checking if DC 
                                       // read 1 coeficinet
        // Read DC and store into mcuData
        if (readComponentElement(scanner, &mcuBuffer,dcTable, 0)) {
            if (!isEndOfScan(scanner, scanner->bytesRead)) {
                printf("ERROR1 reading DC of MCU: %d colorId: %d comp: %d\n",
                    scanner->mcusRead, colorId, comp);
            }
            return 1;
        }
        #ifdef TESTING
            assert(mcuBuffer.acCurrentlyOn == 1);
        #endif
        mcuBuffer.acCurrentlyOn = 0; // corect mstake with acCurrentlyOn
        // Skim past ACs of MCU[comp]
        while (mcuBuffer.acCurrentlyOn < MAX_AC_COEFFICIENTS) {
            // Read component elements until EOB observed or max number of
            // ACs read
            if (readComponentElement(scanner, &mcuBuffer, acTable, 1) ||  
                mcuBuffer.acCurrentlyOn > MAX_AC_COEFFICIENTS) {
                if (!isEndOfScan(scanner, scanner->bytesRead)) {
                    printf("ERROR2 reading AC of MCU: %d colorId: %d comp: %d | coeficients read: %d\n",
                    scanner->mcusRead, colorId, comp, mcuBuffer.acCurrentlyOn);
                }
                return 1;
            }
            if (mcuBuffer.bit == EOB_ENCOUNTERED) {
                #ifdef TESTING
                    assert(mcuBuffer.acCurrentlyOn == MAX_AC_COEFFICIENTS);
                #endif
                break;
            }
        }
    }

    // Read past DC of CB and move onto First AC
    // TODO: make this a process DC function
    colorId = CB_ID - 1;
    dcTable = stats->dcHuffmanTables[colorId];
    acTable = stats->acHuffmanTables[colorId];
    mcuBuffer.acCurrentlyOn = 0;  // So that logic of assert works
    if (readComponentElement(scanner, &mcuBuffer, dcTable, 0)) {
        if (!isEndOfScan(scanner, scanner->bytesRead)) {
            printf("ERROR3 reading DC of MCU: %d colorId: %d comp: %d\n",
                scanner->mcusRead, colorId, stats->colorCounts[colorId]);
        }
        return 1;
    }
    #ifdef TESTING
        assert(mcuBuffer.acCurrentlyOn == 1);
    #endif
    // clear mcuData since starting from scratch
    mcuData->acCurrentlyOn = 0;
    mcuData->bit = 0;
    mcuData->bitLength = 0;
    mcuData->index = 0;
    // Process first AC data of MCU scanner points to and store in mcuData
    if (readComponentElement(scanner, mcuData, acTable, 1) ||
        mcuData->acCurrentlyOn > MAX_AC_COEFFICIENTS || // rest check sanity
        (mcuData->acCurrentlyOn == MAX_AC_COEFFICIENTS &&
        mcuData->bit != EOB_ENCOUNTERED) ||
        (mcuData->bit == ZRL_ENCOUNTERED && mcuData->acCurrentlyOn != 16)) {
        if (!isEndOfScan(scanner, scanner->bytesRead)) {
            printf("ERROR4 reading AC of MCU: %d colorId: %d comp: %d | coeficients read: %d\n",
                scanner->mcusRead, colorId, stats->colorCounts[colorId],
                mcuData->acCurrentlyOn);
            }
        return 1;
    } 

    return 0;
}

/**
 * Frees memory allocated to a scanWorker struct
 */
void destroyScanWorker(scanWorker *scanner) {
    if (scanner == NULL)
        return;
    destroyMCU(scanner->mcu);
    if (scanner->scanBuffer != NULL)
        free(scanner->scanBuffer);
    free(scanner);
}

/**
 * Initialises a scanWorker for probessing a jpeg file using information form 
 * jpegStats to populate data and returns pointer to created scanWorker.
 * Returns NULL if initScanWorker failes
 * 
 * As part of initialization, scanWorker's mcu points to the first AC coeficient
 * of the first CB of the first MCU of the JPG referenced by file
 * 
 * scanner->totalSize is guaranteed to be the length of the part of file not yet
 * read after execution
 * 
 * 
 * After execution, file's cursor is where it was before function call
 * 
 * 
 */
scanWorker* initScanWorker(FILE* file, jpegStats* stats, long fileLength) {
    #ifdef TESTING
        assert(file != NULL && stats != NULL);
    #endif
    scanWorker* scanner = calloc(1, sizeof(scanWorker)); // calloc since most
                                                         // values start at 0
    if (scanner == NULL) {
        return NULL;
    }
    #ifdef TESTING
        // Sanity check on calloc
        assert(scanner->scanBuffer == NULL);
        assert(scanner->mcusRead == 0);
        assert(scanner->bytesRead == 0);
        assert(scanner->bitCursor == 0);
        assert(scanner->mcu == NULL);
        assert(scanner->onSecondChrominance == 0);
    #endif

    // Create byte buffer for image data after SOS and perform sanity checks
    long bytesRead = ftell(file);
    long bytesUnread = fileLength - bytesRead;
    scanner->scanBuffer = calloc(1, bytesUnread); // TODO: make 2x when working with FE bytes
    size_t bufferSize = fread(scanner->scanBuffer, 1, bytesUnread, file);
    #ifdef TESTING
        printf("BYTES READ: %ld bytes\n", bytesRead);
        printf("BYTES UNREAD: %ld bytes\n", bytesUnread);
    #endif
    // Check that enough data was allocated
    if (bufferSize != bytesUnread) {
        printf("ERROR: Expected %ld bytes read, got %ld instead\n", 
                bytesUnread, bufferSize);
        destroyScanWorker(scanner);
        return NULL;
    }
    fseek(file, -bytesUnread, SEEK_CUR);  // set file cursor back for rewriting
    scanner->totalSize = bufferSize;

    // Check that buffer has EOI at end, as expected
    // Note that above makes sure that bufferSize is not larger than size of
    // remaining bytes of file
    if (!isEndOfScan(scanner, bufferSize-2)) {
        puts("ERROR: UNEXPECTED value for last 2 bytes");
        destroyScanWorker(scanner);
        return NULL;
    }

    // Process first MCU of jpg and store pointers to first AC
    scanner->mcu = initMCU(stats);
    if (scanner->mcu == NULL || loadNextMCU(scanner->mcu, scanner, stats)) {
        destroyMCU(scanner->mcu);
        destroyScanWorker(scanner);
        return NULL;
    }
    #ifdef TESTING
        // Expect loadNextMCU to increment mcusRead
        assert(scanner->mcusRead == 1);
    #endif
    scanner->mcusRead--; // no mcu has been completely read yet.
    return scanner;
}

/**
 * Returns 1 if algorithm cannot work with AC currently referenced by mcu and
 * 0 otherwise
 */
int mcuNotPropper(scanWorker *sw, mcu *mcu, jpegStats *stats) {
    #ifdef TESTING
        assert(sw->onSecondChrominance == 0 || sw->onSecondChrominance == 1);
    #endif
    // 1. MCU DC value does not have an EOB/ZRL value and not in range [-1, 1]
    // and not [-3,-2, 2, 3]
    if (mcu->bit == EOB_ENCOUNTERED || mcu->bit == ZRL_ENCOUNTERED ||
        mcu->bitLength <= 1) {
        return 1;
    }
    
    // // 2. Flipping bit of MCU DC cannot turn an FF to FE or FE to FF
    // // TODO: Get rid of this later once handle adding extra 0 at right time
    // unsigned long bufferIndex = mcu->index;
    // if (sw->scanBuffer[bufferIndex] == 0xFF) {
    //     return 1;
    // }

   return 0;
}

/**
 * Given jpegStats scanWorker sw, returns CB_ID - 1 iff coefficient we plan to 
 * read or write from/to is Cb, else return CR_ID-1 if coeficient we plan
 * to do this to is Cr
 * 
 * Assumes in
 */
#define GET_COLOR_INDEX(sw) (CB_ID - 1 + sw->onSecondChrominance)

/**
 * Updates parameters of sw so that 
 *    sw->mcu points to last bit of (Color) AC value right after current one
 *        or indicates that next AC does not have a bit (is "EOB" or ZRL)
 *    sw->onSecondChrominance is updated if current Chrominance value fully
 *        read
 *    sw->mcu->acCurrentlyOn is propperly incremented or set to 0 when 
 *        appropriate
 * 
 * Assumes that if EOB is read, mcu->acCurrentlyOn becomes MAX_AC_COEFFICIENTS
 *      
 */
int advanceMCUPointer(scanWorker *sw, jpegStats *stats) {
    if (sw->mcu->acCurrentlyOn == MAX_AC_COEFFICIENTS) {
        // Move onto next MCU if at last chrominance
        if (sw->onSecondChrominance) {
            sw->onSecondChrominance = 0;
            return loadNextMCU(sw->mcu, sw, stats);
            
        } else {
            // Prepare to read first AC of second (CR) color component
            sw->onSecondChrominance = 1;
            // Get past DC of second color
            int colorIndex = GET_COLOR_INDEX(sw);
            dhtTrie *dcTable = stats->dcHuffmanTables[colorIndex];
            readComponentElement(sw, sw->mcu, dcTable, 0);
            // Clear data read by readComponentElement
            sw->mcu->bit = 0;
            sw->mcu->index = 0;
            sw->mcu->acCurrentlyOn = 0;
            sw->mcu->bitLength = 0;
        }
    }

    // If not moveing to next MCU, read AC of this MCU using right data
    int colorIndex = GET_COLOR_INDEX(sw);
    dhtTrie *acTable = stats->acHuffmanTables[colorIndex];
    readComponentElement(sw, sw->mcu, acTable, 1);
    return 0;
}

/**
 * Code for increasing the size of the buffer if program just converted a byte
 * in sw->scanBuffer to 0xFF, the one referenced by mcu.
 */
int growScanBuffer(scanWorker *sw, mcu *mcu) {
    #ifdef TESTING
        assert(sw->bytesRead < sw->totalSize);
        assert(sw->scanBuffer[mcu->index] == 0xFF);
        assert((sw->bytesRead == mcu->index && sw->bitCursor == 1+mcu->bit) || 
               (sw->bitCursor == 0 && mcu->bit == 7 && 
               sw->bytesRead > mcu->index));
    #endif
    unsigned long oldSize = sw->totalSize;
    sw->totalSize += 1;
    sw->scanBuffer = realloc(sw->scanBuffer, sw->totalSize);
    if (sw->scanBuffer == NULL) {
        puts("ERROR REALLOCATING BUFFER OF SW");
        return 1;
    }
    memcpy(&sw->scanBuffer[mcu->index + 2], &sw->scanBuffer[mcu->index + 1], 
           oldSize - (mcu->index+1));
    sw->scanBuffer[mcu->index + 1] = 0;

    if (sw->bytesRead > mcu->index) {
        #ifdef TESTING
            assert(sw->bitCursor == 0 && mcu->bit == 7);
        #endif
        sw->bytesRead++;
    }

    return 0;
}

/**
 * Code for decreasing the size of the buffer if program just converted a byte
 * in sw->scanBuffer from 0xFF to another value, the one referenced by mcu.
 */
int shrinkScanBuffer(scanWorker *sw, mcu *mcu) {
    #ifdef TESTING
        assert(sw->bytesRead < sw->totalSize);
        assert(sw->scanBuffer[mcu->index] != 0xFF);
        assert((sw->bytesRead == mcu->index && sw->bitCursor == 1+mcu->bit) || 
               (sw->bitCursor == 0 && mcu->bit == 7 && 
               sw->bytesRead > mcu->index));
    #endif
    unsigned long oldSize = sw->totalSize;
    memcpy(&sw->scanBuffer[mcu->index + 1], &sw->scanBuffer[mcu->index + 2], 
           oldSize - (mcu->index+2));
    sw->totalSize -= 1;
    sw->scanBuffer = realloc(sw->scanBuffer, sw->totalSize);
    if (sw->scanBuffer == NULL) {
        puts("ERROR REALLOCATING BUFFER OF SW");
        return 1;
    }
    // Make sw->bytesRead and bit cursor equal to the bit right after bit 
    // referneced by mcu
    sw->bytesRead = mcu->index;
    sw->bitCursor = mcu->bit;
    nextBit(sw);
    // Done
    return 0;
}


/**
 * Writes bit onto last bit of AC pointed to by mcu inside sw->scanBuffer
 * under assumptoin that mcu is propper, !mcuNotPropper(sw, sw->mcu, *)
 * 
 * Returns 0 on success and 1 on failiure
 * 
 * Assumes that mcu points to the data bit right before the one referenced by 
 * sw's pointers
 */
void performWrite(scanWorker *sw, mcu *mcu, unsigned char bit) {
    unsigned long index= mcu->index;
    unsigned char bitIndex = mcu->bit;
    unsigned char shift = 7 - bitIndex;
    unsigned char mask = 1 << shift;
    #ifdef TESTING
        assert(IS_BIT((sw->scanBuffer[index] & mask) >> shift));
    #endif
    unsigned char byteBeforeChange = sw->scanBuffer[index]; // Used for shrinking check
    if ((sw->scanBuffer[index] & mask) >> shift != bit) {
        // Actually make a change
        sw->scanBuffer[index] = sw->scanBuffer[index] ^ mask;
        if (sw->scanBuffer[index] == 0xFF) {
            // Grow buffer
            #ifdef TESTING
                assert(byteBeforeChange != 0xFF);
            #endif
            growScanBuffer(sw, mcu);
        } else if (byteBeforeChange == 0xFF) {
            #ifdef TESTING
                assert(sw->scanBuffer[index] != 0xFF);
            #endif
            shrinkScanBuffer(sw, mcu);
        }
    } // else no need to modify coeficient
}

/**
 * Returns the bit at index mcu->index, mcu->bit in sw->scanBuffer
 * Assumes sw and mcu point to valid bit in sw->scanBuffer
 */
unsigned char performRead(scanWorker *sw, mcu  *mcu) {
    unsigned long index = mcu->index;
    unsigned char bitIndex = mcu->bit;
    unsigned char shift = 7 - bitIndex;
    unsigned char mask = 1 << shift;

    unsigned char bitRead = (sw->scanBuffer[index] & mask) >> shift;
    #ifdef TESTING
        printf("BIT READ: %d\n", bitRead);
    #endif
    return bitRead;
}

/**
 * Processes a last bit of the AC coeficient referenced by
 * sw->mcu in sw->scanBuffer. if bit stores value READ_MESSAGE_CODE_PROCESSOR, 
 * the data pointed to is read, otherwise, it is modified to the contents of
 * bit. stats is used to parse the scan data in sw.
 * 
 * Returns 0 on success and 1 on failiure
 * 
 * Assumes sw's metadata (onSecondChrominance and mcu) always 
 * reference the latest, unread AC coeficient after each line.
 * 
 */
int processBit(scanWorker *sw, jpegStats *stats, unsigned char *bit) {
    #ifdef TESTING
        printf("BIT VALUE %d\n", *bit);
        assert(*bit == 0 || *bit == 1 || *bit == READ_MESSAGE_CODE_PROCESSOR);
    #endif
    // Use fact that only use Y,Cr,Cb with only 1 Cr and Cb entry per MCU
    // Find a decent chrominance value to work with
    while (mcuNotPropper(sw, sw->mcu, stats)) {
        if (advanceMCUPointer(sw, stats)) {
            return 1;
        }
    }

    #ifdef TESTING
        printf("WORKING WITH BIT %ld %d \n", sw->mcu->index, sw->mcu->bit);
    #endif
    // Write/Read bit and move cursor past AC just modified
    if (*bit == READ_MESSAGE_CODE_PROCESSOR) {
        *bit = performRead(sw, sw->mcu);
    } else {
        performWrite(sw, sw->mcu, *bit);
    }
    if (advanceMCUPointer(sw, stats)) {
        return 1;
    }
    return 0;
}

/**
 * Finds the number of characters (minus ending 0 byte)
 * that can be written into jpeg file with jpegStats stats and size fileLength.
 * After this is executed, file's cursor will remain unchanged, as well as its
 * contents
 *
 * Assumes file's cursor is right after SOS segment (points to first bit of
 * actual, quantized data).
 */
long getMaxMessageSize(FILE *file, jpegStats *stats, long fileLength) {
    #ifdef TESTING
        puts("Reading message from JPEG");
    #endif
    scanWorker *sw = initScanWorker(file, stats, fileLength);
    if (sw == NULL) {
        return -1;
    }

    // Get number of bits that are readable
    long counter = 0;
    while (sw->bytesRead < sw->totalSize) {
        // Read bit and append to buffer
        unsigned char bitRead = READ_MESSAGE_CODE_PROCESSOR;
        if (processBit(sw, stats, &bitRead)) {
            if (isEndOfScan(sw, sw->bytesRead)) {
                break;
            } else {
                destroyScanWorker(sw);
                return -1;
            }
        }
        counter++;
        #ifdef TESTING
            assert(IS_BIT(bitRead));
        #endif
    }
    #ifdef TESTING
        printf("FINAL ON MCUS READ: %d\n", sw->mcusRead);
        printf("TOTAL MCUS IN FILE: %d\n", stats->mcuCount);
    #endif
    destroyScanWorker(sw);
    return counter / 8 - 1;
}

/**
 * Reads hidden message in SOS of jpeg file file with data stored in stats and
 * of size fileLength bytes. Returns the message on success and returns NULL if
 * a failiure is detected.
 *
 * Assumes that file cursor points to first bit of actual SOS data and that a
 * message was hidden in file
 */
char* scannerReadMessage(FILE *file, jpegStats *stats, long fileLength) {
    #ifdef TESTING
        puts("Reading message from JPEG");
    #endif
    scanWorker *sw = initScanWorker(file, stats, fileLength);
    if (sw == NULL) {
        return NULL;
    }

    // Get message
    size_t bufferSize = 16;
    size_t counter = 0;
    char *mssg = malloc(bufferSize);
    if (mssg == NULL) {
        destroyScanWorker(sw);
        return NULL;
    }
    // Read bytes of hidden message bit by bit
    while (sw->bytesRead < sw->totalSize) {
        char dataBuffer = 0;
        for (int i = 0; i < 8; i++) {
            // Read bit and append to buffer
            unsigned char bitRead = READ_MESSAGE_CODE_PROCESSOR;
            processBit(sw, stats, &bitRead);
            #ifdef TESTING
                assert(IS_BIT(bitRead));
            #endif
            dataBuffer = dataBuffer | (bitRead << (7 - i));
        }
        #if TESTING
            printf("DATA READ FROM JPEG FILE: %c\n", dataBuffer);
        #endif
        mssg[counter] = dataBuffer;
        if (dataBuffer == 0) {
            break;
        }
        // Increment counter and double bufferSize if need be
        counter++;
        if (counter == bufferSize) {
            bufferSize = 2*bufferSize;
            mssg = realloc(mssg, bufferSize);
            if (mssg == NULL) {
                destroyScanWorker(sw);
                return NULL;
            }
        }
    }
    #ifdef TESTING
        printf("FINAL ON MCUS READ: %d\n", sw->mcusRead);
        printf("TOTAL MCUS IN FILE: %d\n", stats->mcuCount);
    #endif
    destroyScanWorker(sw);
    return mssg;
}

/**
 * Writes the content of sw into file jpg at the current cursor, assumed to be
 * the position immediately following SOS segment
 * 
 * Returns 0 iff successful and 1 otherwise
 */
int modifyFile(FILE* jpg, scanWorker *sw) {
    size_t bytesWritten = fwrite(sw->scanBuffer, 1, sw->totalSize, jpg);
    return bytesWritten == sw->totalSize;
}

/**
 * Hides a message inside the LSBs of propper AC coeficients of file of size
 * fileLength bytes and data stored in stats
 * 
 * Assuems file points to first byte of scan data and that stats contains data
 * extracted from file
 */
int scannerHideMessage(FILE *file, jpegStats *stats, char *message, 
                       long fileLength) {
    #ifdef TESTING
        printf("\nHideing message [%s] in JPEG\n", message);
    #endif
    scanWorker *sw = initScanWorker(file, stats, fileLength);
    if (sw == NULL) {
        return 1;
    }

    // Hide message
    size_t mssgSize = strlen(message)+1;
    for (size_t i = 0; i < mssgSize; i++) {
        for(unsigned char j = 0; j < 8; j++) {
            unsigned char shift = 7 - j;
            unsigned char bit = (message[i] & (1 << shift)) >> shift;
            #ifdef TESTING
                assert(bit == 0 || bit == 1);
            #endif
            if (processBit(sw, stats, &bit)) {
                destroyScanWorker(sw);
                return 1;
            }
        }
    }
    int result = modifyFile(file, sw);
    #ifdef TESTING
        printf("FINAL ON MCUS READ: %d\n", sw->mcusRead);
        printf("TOTAL MCUS IN FILE: %d\n", stats->mcuCount);
    #endif
    destroyScanWorker(sw);
    return result;
}
