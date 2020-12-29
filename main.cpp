#include "mbed.h"
#include <stdio.h>
#include <errno.h>
#include <functional>
#include <chrono>
#include "MODDMA.h"
#define BUFFER_MAX_LEN 10
#define FORCE_REFORMAT true

#include "SDBlockDevice.h"
BlockDevice *sd = SDBlockDevice::get_default_instance();
#include "FATFileSystem.h"
FATFileSystem fs("fs");


volatile uint16_t fileBuffer = NULL; 
int bufferPtr = 0;

AnalogIn adcz(A0);
Ticker ticker;
Timer fileOpenTimer;
 
#define bufferSize 4096
uint16_t sensorReading[bufferSize];
unsigned int readPointer = 0;
volatile unsigned int writePointer = 0; // volatile so that the main loop knows to check for changes.

// opens the next unused file name in the format set.
// This can be a little slow the first time if there are already lots of log files
// since it tries each number in turn but the second call on will be fairly quick.
int counter = 0;
FILE *nextLogFile(void)
{
    static unsigned int fileNumber = 0;
    char fileName[32];
    FILE *filePtr = NULL;
    do {
        if (filePtr != NULL)
            fclose(filePtr);
        sprintf(fileName,"/fs/log%04u.txt",counter++);
        filePtr = fopen(fileName,"r");
    } while (filePtr != NULL);
    return fopen( fileName,"w");
}

void onSampleTick(void)
{
    sensorReading[writePointer++] = adcz.read_u16(); 
    if (writePointer == bufferSize)
        writePointer = 0;
    if (writePointer == readPointer) {
        //Overflow
    }
}
 


int main()
{
    int err = fs.mount(sd);
    if (err || FORCE_REFORMAT) {
        printf("formatting... ");
        fflush(stdout);
        err = fs.reformat(sd);
        if (err) {
            printf("badreform");
            error("error: %s (%d)\n", strerror(-err), err);
            return 1;
        }
    }
    FILE *myLogFile;
    myLogFile = nextLogFile();
    if (!myLogFile) {
        printf("notlog");
        // ERROR failed to open the first log file for writing.
        // The SD card is missing, not working, read only or full?
 
        return 1; // probably want to exit the program in this situation
    }
    fileOpenTimer.start();
    

    ticker.attach(&onSampleTick,0.0001);

while(true)
{
        while (writePointer != readPointer) { // write any waiting data to the SD card
            fprintf(myLogFile,"%hu\r\n",sensorReading[readPointer++]);
            if (readPointer == bufferSize)
                readPointer = 0;
        }
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(fileOpenTimer.elapsed_time()).count() > 1000*60) {
            fclose(myLogFile); // close the current file
            printf("done");
            myLogFile = nextLogFile(); // open a new file
            if (!myLogFile) {
                // ERROR failed to open the log file for writing.
                break; // exit the while(true) loop
            }
            fileOpenTimer.reset(); // restart the timer
        }
}
}

