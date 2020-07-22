#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "csteg.h"
#include "scanWorker.h"

#ifdef TESTING
    #include <assert.h>
#endif

/**
 * Assumes:
 *      system is little endian
 */


void destroyJpegStats(jpegStats* x) {
    for (int i = 0; i < 3; i++) {
        if (x->dcHuffmanTables[i] != NULL) 
            destroyDhtTrie(x->dcHuffmanTables[i]);
            // Prevent freeing of same address referenced elsewhere
            for(int j = i+1; j < 3; j++) {
                if (x->dcHuffmanTables[i] == x->dcHuffmanTables[j])
                    x->dcHuffmanTables[j] = NULL;
            }
        if (x->acHuffmanTables[i] != NULL) 
            destroyDhtTrie(x->acHuffmanTables[i]);
            // Prevent freeing of same address referenced elsewhere
            for(int j = i+1; j < 3; j++) {
                if (x->acHuffmanTables[i] == x->acHuffmanTables[j])
                    x->acHuffmanTables[j] = NULL;
            }
    }
    free(x);
}

/**
 * Non-exhaustive check to assure that the file fileName is a JPEG file.
 * 
 * Assumes filePointer is set to the begining of file fileName
 * And assumes this is executed in little-endian system.
 * 
 * returns 0 if filePointer is a valid JPEG file and 1 otherwise
 * also moves the files cursor past first 2 bytes of filePointer 
 */
int isNotJPEG(char *fileName, FILE *filePointer ) {
    // Make short equal to value read fo fgets()
    unsigned short jpegStart = 0;
    fgets( ((char*)&jpegStart), 3, filePointer);
    jpegStart = BYTE_TO_SHORT_VALUE(jpegStart);

    return jpegStart != JPEG_START;
}

/**
 * Sets *jpgetStatsHolderHelper to a jpegStats struct with data provided from
 * SOF-0 section. Serves as helper for getSOF0Data(), where length, height, and
 * numberOfComponentes of image jpegFile originate form.
 */
int populatejpegStats(jpegStats** jpegStatsHolder, FILE* jpegFile,
                        unsigned short length, unsigned short height, 
                        unsigned short numberOfComponents) {
                            
    (*jpegStatsHolder)->totalColorCounts = 0;
    unsigned char component_data[3*numberOfComponents];
    fread(component_data, 3, numberOfComponents, jpegFile);
    // Calculate number of components of each color value per MCU
    for(char i = 0; i < numberOfComponents; i++) {
        unsigned char colorId = component_data[3*i];
        if (INVALID_COLOR(colorId)) {
            puts("ERROR: Unsupported color scheme");
            return 1;
        }
        // components for determinnig color value
        unsigned char horizontal = component_data[3*i+1] >> 4;
        unsigned char vertical = component_data[3*i+1] & 15; // 15 = 0b1111
        (*jpegStatsHolder)->colorCounts[colorId - 1] = vertical * horizontal;
        (*jpegStatsHolder)->totalColorCounts += 
            (*jpegStatsHolder)->colorCounts[colorId - 1];
        
        if (colorId != Y_ID &&
            (*jpegStatsHolder)->colorCounts[colorId - 1] != 1) {
            puts("WARNING: Untested sampling methedology");
        }
        // TODO: Review: ignore quantiziation table information
        #ifdef TESTING
            printf("\tCOMPONENT_DATA: %d %dx%d %d \n",
                    component_data[3*i], horizontal, vertical, 
                    component_data[3*i+2]
            );
        #endif
    }
    // Calculate total number of MCUs in image
    unsigned int totalPixels = length * height;
    unsigned int mcuSize = STD_MCU_SIZE * 
                             (*jpegStatsHolder)->colorCounts[Y_ID - 1];
    #ifdef TESTING
        // Make sure that mcuSize makes sense for image
        assert(totalPixels % mcuSize == 0);
    #endif
    
    (*jpegStatsHolder)->mcuCount = totalPixels / mcuSize;
    return 0; // all is well
}

/**
 * Get info from START OF FRAME-0 SEGMENT while advancing cursor
 */ 
int getSOF0Data(FILE *jpegFile, unsigned short segment_length, 
                jpegStats** jpegStatsHolder) {
    fseek(jpegFile, 1, SEEK_CUR); // skip precission value
    
    // Get length and width for future use
    unsigned short length;  // lenght/width of jpeg (in piexls)
    fread(&length, 2, 1, jpegFile);
    length = BYTE_TO_SHORT_VALUE(length);
    unsigned short height;  // height of jpeg (in pixels)
    fread(&height, 2, 1, jpegFile);
    height = BYTE_TO_SHORT_VALUE(height);
    
    unsigned char n;  // number of components in frame
    fread(&n, 1, 1, jpegFile);
    
    #ifdef TESTING
        // check if segment_length has expected value and print data
    	printf("\tSOF-0 DATA: %d %d %d \n", length, height, n);
        assert(segment_length == 3*n + 8);
    #endif
    
    if (n != 3 || 
        populatejpegStats(jpegStatsHolder, jpegFile, length, height, n)) {
        return 1;
    }

    return 0;
}

/**
 * Populates jpegStats struct with the data in the Restart Interval of
 * jpegFile, which has its length property set to segment_length
 */
int getRestartData(FILE *jpegFile, unsigned short segment_length, 
                   jpegStats *jpegStats) {
    #ifdef TESTING
        assert(segment_length == 4);
    #endif 

    fread(&(jpegStats->restartInterval), 2, 1, jpegFile);

    jpegStats->restartInterval = BYTE_TO_SHORT_VALUE(jpegStats->restartInterval);
    #ifdef TESTING
        printf("\tRESTART INTERVAL: %d\n", jpegStats->restartInterval);
    #endif
    return 0;

}

/**
 * Populates the huffman table data of jpegStats using contents of jpegFile's
 * SOS segment and tables stored in tables. Also checks to see whether SOS
 * segment is propperly formatted, including whether it's length matches the
 * expected segment_length, as previously read.
 *  
 * Serves as helper for setFileCurosor().
 * 
 * Assumes that jpegFile cursor is right after the length component of a SOS
 * segment.
 */
int matchColorsToTables(FILE* jpegFile, jpegStats* jpegStats, 
                        dhts* tables, unsigned short segment_length) {
    unsigned char n;
    fread(&n, 1, 1,jpegFile);
    if (n != 3 || segment_length != 2 + 1 + 2*n + 3) {
        puts("ERROR: Invalid SOS segment");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        unsigned char colorData[2]; // stores id,huffmanTable data respectively
        fread(colorData, 1, 2, jpegFile);
        // Check if color id is Y, Cr, or Cb and is a new color
        if (colorData[0] < 1 || colorData[0] > 3 || 
            jpegStats->dcHuffmanTables[colorData[0] - 1] != NULL) {
            puts("ERROR: Invalid SOS segment");
            return 1;
        }

        #ifdef TESTING
            printf("\tCOLOR DATA: %u %u\n", colorData[0], colorData[1]);
        #endif

        colorData[0]--;  // make id match expectations of jpegStats
        // Claculate indecies of current color's DC and AC tables in tables
        // and store tables->tables[index] in jpegStats
        unsigned char dcId = 2*(colorData[1]>>4);
        if (dcId >= MAX_NUMBER_OF_TABLES || tables->tables[dcId] == NULL) {
            puts("ERROR: Issue with processing Huffman Table Data");
            return 1;
        }
        jpegStats->dcHuffmanTables[colorData[0]] = tables->tables[dcId];
        
        unsigned char acId = 2*(colorData[1]&15)+1;
        if (acId >= MAX_NUMBER_OF_TABLES || tables->tables[acId] == NULL) {
            puts("ERROR: Issue with processing Huffman Table Data");
            return 1;
        }
        jpegStats->acHuffmanTables[colorData[0]] = tables->tables[acId];
    }

    fseek(jpegFile, 3, SEEK_CUR);  // ignore last 3 skip bytes
    return 0;
}

/**
 * Moves the cursor of jpegFile to the byte immediately after the Start of 
 * Scan (SOS) flag of the SOS section. Also initialises dhts object, storeing
 * it in dhtTables.
 * 
 * Assumes jpegFile is pointing to the third byte of a JPEG file
 * 
 * returns 0 on success some other value if failiure
 */ 
int setFileCursor(FILE *jpegFile, dhts** dhtTables, 
                  jpegStats** jpegStatsHolder) {
    unsigned short buffer[3]; // Store section marker and length respectively.
    *jpegStatsHolder = malloc(sizeof(jpegStats));

    fread((void*)&buffer, 1, 4, jpegFile);
    buffer[0] = BYTE_TO_SHORT_VALUE(buffer[0]);
    buffer[1] = BYTE_TO_SHORT_VALUE(buffer[1]);
    while(buffer[0] != JPEG_SOS) {
        #ifdef TESTING
	        printf("SECTION: %hX | LENGTH-ENTRY: %u\n", buffer[0], buffer[1]);
	    #endif

        int result = 0;
        switch(buffer[0]) {
            case START_OF_FRAME_0 :
                result = getSOF0Data(jpegFile, buffer[1], jpegStatsHolder);
                break;
            case DRI_MARKER :
                result = getRestartData(jpegFile, buffer[1], *jpegStatsHolder);
                break;
            case DHT_START:
                fseek(jpegFile, -4, SEEK_CUR);
                *dhtTables = createDhts(jpegFile);
                result = *dhtTables == NULL;
                break;
            default :
                result = fseek(jpegFile, buffer[1] - MARKER_LENGTH, SEEK_CUR);
                break;
        }

        // Return 1 if error occurred
        if (result) {
            if (jpegStatsHolder != NULL) 
                free(*jpegStatsHolder);
            destroyDhts(*dhtTables);
            return 1;
        }

        // Get next marker and length data
        fread((void*)&buffer, 1, 4, jpegFile);
        buffer[0] = BYTE_TO_SHORT_VALUE(buffer[0]);
        buffer[1] = BYTE_TO_SHORT_VALUE(buffer[1]);
    }

    // Get data to match colors to tables
    #ifdef TESTING
	    printf("SECTION: %hX | LENGTH-ENTRY: %u\n", buffer[0], buffer[1]);
	#endif
    return matchColorsToTables(jpegFile, *jpegStatsHolder, *dhtTables,buffer[1]);
}

/**
 * Advances imgFile cursor to SOS, storing pertinent info of said jpegFile
 * in jpegStats. Assumes filePath references the file imgFile, which has byte
 * read permissions
 */
jpegStats* getJpegStats(char *filePath, FILE *imgFile) {
    dhts* dhtTables = NULL;  // pointer to dhtTable data of imgFile
    jpegStats* jpegStats = NULL;

    // Check if filePath is a jpeg and move cursor to SOS of JPG 
    if(isNotJPEG(filePath, imgFile) || 
       setFileCursor(imgFile, &dhtTables, &jpegStats)) {
        // If either JPEG test or cursor setting fails,
	    // Clear allocated data and return 1 (error)
        printf("ISSUE with jpeg file: %s\n", filePath);
        if(dhtTables != NULL) {
            destroyDhts(dhtTables);
        }
        if (jpegStats != NULL) {
            destroyJpegStats(jpegStats);
        }
	    fclose(imgFile);
        return NULL;
    }

    free(dhtTables);  // Don't destroy since contents of dhtTables still used
    return jpegStats;

}

/**
 * Given the name of a file, calculate its length in bytes.
 *
 * Assumes filePath exists
 */
long getFileSize(char* filePath) {
    FILE *file = fopen(filePath, "rb");
    fseek (file, 0, SEEK_END);
    long length = ftell(file);
    fclose(file);

    #ifdef TESTING
        printf("FILE SIZE: %ld bytes\n\n", length);
    #endif
    return length;
}

/**
 * Asks user to type in a message and returns a string containing the first
 * maxMessageSize bytes of that message
 * and returns that message in allocated memory.
 */
char* askForMessage(char* filePath, long maxMessageSize) {
    printf("Write a message to hide in %s with a maximum of %ld characters:\n",
            filePath, maxMessageSize);
    // Use calloc to make sure that NULL is in array
    char* mssg = (char*)calloc(maxMessageSize+1, 1);  // +1 for string end
    if (fgets(mssg, maxMessageSize+1, stdin)) {
        mssg[maxMessageSize] = 0;
        return mssg;
    }
    return NULL;
}

/**
 * Reads data from filePath and stores at most maxMessageSize bytes in char
 * array to return. Returns NULL if error occurred
 */
char* loadMessage(char* filePath, long maxMessageSize) {
    printf("Loading at most %ld characters from %s:\n",
            maxMessageSize, filePath);
    // Use calloc to make sure that NULL is in array
    size_t allocSize = maxMessageSize+1;
    char* mssg = (char*)calloc(allocSize, 1);  // +1 for 0 btye end
    if (mssg == NULL) {
        printf("ERROR allocating space of size %ld", allocSize);
        return NULL;
    }
    FILE *mssgFile = fopen(filePath, "r");
    if (mssgFile == NULL) {
        printf("ERROR reading file %s\n", filePath);
        free(mssg);
        return NULL;
    }

    // Read data from mssgFile into mssg buffer, then check if successful
    size_t bytesRead = fread(mssg, 1, maxMessageSize, mssgFile);
    if (bytesRead != maxMessageSize && !feof(mssgFile)) {
        printf("ERROR reading file %s\n", filePath);
        free(mssg);
        return NULL;
    }

    mssg[allocSize-1] = 0;  // Do this to ensure end of string
    #ifdef TESTING
        printf("MESSAGE TO HIDE:\n%s\n", mssg);
    #endif
    return mssg;
}


int extractMessage(char *imgFilePath, char *outputFile) { 
    FILE *imgFile = fopen(imgFilePath, "rb");
     jpegStats *jpegStats = getJpegStats(imgFilePath, imgFile);
    if (jpegStats == NULL) {
        return 1;
    }

    // Obtain hidden message and write to outputFile
    long fileSize = getFileSize(imgFilePath);
    char *hiddenMessage = scannerReadMessage(imgFile, jpegStats, fileSize);
    if (hiddenMessage == NULL) {
        destroyJpegStats(jpegStats);
        return 1;
    }
    FILE *out = fopen(outputFile, "w");
    fprintf(out, "EXTRACTED MESSAGE FROM [%s]:\n%s\n\n", imgFilePath,
            hiddenMessage);
    fclose(out);

    free(hiddenMessage);
    fclose(imgFile);
    destroyJpegStats(jpegStats);
    return 0;
}

/**
 * Hides a user-defined message (from inputFilePath text file or stdin if
 * inputFilePath is NULL) inside JPG pointed to by filePath, modifying
 * that exact file. input is NULL; only used to to match type of
 * operation variable in main()
 */
int hideMessage(char* filePath, char* inputFilePath) {
    FILE *imgFile = fopen(filePath, "r+b");  // pointer to jpg file
    jpegStats *jpegStats = getJpegStats(filePath, imgFile);
    if (jpegStats == NULL) {
        return 1;
    }
    long fileSize = getFileSize(filePath);
    puts("Loading Max Message Size");
    long maxMessageSize = getMaxMessageSize(imgFile, jpegStats, fileSize);
    if (maxMessageSize <= 0) {
        printf("ERROR Loading max message size\n");
        destroyJpegStats(jpegStats);
        return 1;
    }

    // Get message and hide it in imgFile
    char*(*obtainMssg)(char*,long) = inputFilePath ? loadMessage: askForMessage;
    char *message = obtainMssg(inputFilePath, maxMessageSize);
    if (message) {
        scannerHideMessage(imgFile, jpegStats, message, fileSize);
    }

    // free alloced space
    free(message);
    fclose(imgFile);
    destroyJpegStats(jpegStats);
    return message == NULL; // Do check on obtainMssg success here!
}

/**
 * Returns 1 if the file referenced by filePath exists and 0 otherwise
 */
int fileExists(char *filePath) {
    FILE *file = fopen(filePath, "r");
    if (file != NULL) {
        fclose(file);
        return 1;
   } else {
        return 0;
   }
}

/**
 * Returns 0 if the parameters passed into csteg.c are valid and also set
 * *tag (determines read or write), *jpgFile (path to image in which to perform
 * read or write), and *mssgFilePath (where to write or read message into)
 * to specified values, otherwise return 1
 *
 * Assumes that if argv[i] is the address to an actual string for i in [0, argc)
 * and that if, while reading a message and the text parameter is set, the
 * parent of the specified path actually exists.
 */
int checkArgs(int argc, char **argv, char **tag, char **jpgFile,
                char **mssgFilePath) {
    // Make sure right number of arguments
    if (argc <= 2) {
        printf("ERROR: should be executed with at least %d parameters and at most %d.\n",
                2, 3);
        return 1;
    }
    // Check that tag is valid
    *tag = argv[1];
    if ((*tag)[0] != '-' || ((*tag)[1] != 'w' && (*tag)[1] != 'r')) {
        printf("ERROR: invalid tag %s, %c\n", *tag, *tag[1]);
        return 1;
    }
    // Basic check on jpeg argument
    // Make sure file ends in a jpeg extenssion and that it exists
    *jpgFile = argv[2];
    size_t dotIndex = strlen(*jpgFile) - 1;
    for (; dotIndex > 0; dotIndex--) {
        if ((*jpgFile)[dotIndex] == '.')
            break;
    }
    if ((*jpgFile)[dotIndex] != '.' ||
        (strcmp(&((*jpgFile)[dotIndex]), ".jpg") != 0 &&
        strcmp(&((*jpgFile)[dotIndex]), ".jpeg") != 0 &&
        strcmp(&((*jpgFile)[dotIndex]), ".jpe") != 0 &&
        strcmp(&((*jpgFile)[dotIndex]), ".jfif" ) != 0) ||
        !fileExists(*jpgFile)) {
        printf("ERROR: Invalid image file path %s\n \tMake sure the file exists and is a jpg\n",
               *jpgFile);
        return 1;
    }
    // Do check on optional message text file
    // Make sure it has .txt extenssion and exists if tag is -w
    *mssgFilePath = argc == 2 ? NULL : argv[3];
    if (*mssgFilePath != NULL) {
        // Extenssion checks
        if (strlen(*mssgFilePath) < 4) {
            printf("ERROR: Invalid text file %s\n", *mssgFilePath);
            return 1;
        }
        char *ending = &((*mssgFilePath)[strlen(*mssgFilePath) - 4]);
        if (strcmp(ending, ".txt") != 0) {
            printf("ERROR: Invalid text file %s\n", *mssgFilePath);
            return 1;
        }
        // Existance check
        if (*tag[1] == 'w' && !fileExists(*mssgFilePath)) {
            printf("ERROR: If hiding a message, %s must exist\n",*mssgFilePath);
            return 1;
        }
    }

    return 0;
}

// TODO: More development on hide message functionality, consider moving main 
//       to seperate file, to handle presentation

/**
 * Checks to make sure that command entered by user is valid and executes it.
 */
int main(int argc, char** argv) {
    
    char* tag;           // command parameter to use, should be argv[1]
    char* mssgFilePath;  // jpg parameter, should be argv[2]
    char* imgFileName;   // txt parameter, should be NULL or argv[3]
    if (checkArgs(argc, argv, &tag, &imgFileName, &mssgFilePath)) {
        return 1;
    }

    int (*operation)(char*,char*);
    switch(tag[1]) {
        case 'r':  // read/extract  message from file
            if (mssgFilePath == NULL) {
                mssgFilePath = "extracted_messages.txt";
            }
            operation = &extractMessage;
            break;
        case 'w':  // write/hide message in file
            operation = &hideMessage;
            break;
        default:
            // Should never happen because of checkArgs
            return 1;
    }
    
    if((*operation)(imgFileName, mssgFilePath)) {
        printf("WARNING, %s failed for %s\n", tag, imgFileName);
    }
    else {
        printf("COMPLETED TASK FOR %s\n", imgFileName);
    }

    return 0;
}
