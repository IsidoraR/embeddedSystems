// To compile: gcc dht11_interr.c -o dht11_interr -lwiringPi -Wall

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <stdint.h>
#include <time.h>

#include "wiringPi.h"

typedef enum State
{
	INIT_PULL_LINE_LOW,
	INPUT_JUST_ENABLED,
	HIGH_ACK,
	BIT_READ_RISING,
	READ_COMPLETE,
	ERROR_STATE
} State;



static volatile State currentState = READ_COMPLETE;
static volatile int currentReadingBitIdx = 0;
static int prevBitHighTime;
static unsigned int microsCurrentRisingEdge = 0;
static volatile int microsPreviousRisingEdge = 0;
static uint8_t bitsRcvd[41];
static uint8_t dht11_dat[5] = {0,0,0,0,0};
static volatile bool readReady = false;
static int  measuredBitHighTime[40];

const int SENSOR_PIN_NUM = 7;
const int RESPONSE_TIME_US = 80;
const int PRE_BIT_DELAY_US = 50;
const int MAX_TIME_FOR_ZERO_BIT_US = 28;
const int MAX_TIME_FOR_ONE_BIT_US = 70;
const int MAX_TIME_BUFFER = 10;
const int BITS_PER_BYTE = 8;
const char * const OUTPUT_FILENAME = "./temperatureandhumidityrecords.txt";
#define TOTAL_BITS_PER_READ 40


void sensorReadISR(void);
void setupGpio(void);
void initiateRead(void);
void releaseGpio(void);
void analyzeAndPrintResults(int* bitsRcvd, const char* errorString, const char* sensorInteractionMode);
char * getTimeAsString(void);

void writeResultsToFile(int temp_int, int temp_dec, 
						int humid_int, int humid_dec,
						int checksumRead, int checksumGen,
						const char * sensorInteractionMode,
						const char * timeAsString,
						const char * errorString);

// Global Initializations
bool readSuccessful = false;

// We use debug mode to print timings associated with the 
// measurements.
//#define DEBUG_MODE
#ifdef DEBUG_MODE
	int numUsWaitingForSensorResponse = 0;
	int numUsLowSensorResponse = 0;
	int numUsHighSensorResonse = 0;
	int numUsLowPreBit[40];
	int numUsForBit[40];
	int bitNum = 0;
#endif

int main()
{
	if (piHiPri(99) == -1)
	{
		printf("Error setting priority! Exiting");
		return -1;
	}
		
	setupGpio();
	
	if (wiringPiISR(SENSOR_PIN_NUM, INT_EDGE_RISING, sensorReadISR) < 0)
	{
		printf("ERROR SETTING ISR!\n");
		currentState = ERROR_STATE;
	}		

	int readIdx = 0;
	while (!readSuccessful && readIdx < 100)
	{
		printf("\nSample Number: %i\n", readIdx);
		
		memset((int *)bitsRcvd, 0, TOTAL_BITS_PER_READ * sizeof(int));
		memset((int *)measuredBitHighTime, 0, TOTAL_BITS_PER_READ * sizeof(int));

		readReady = false;
	
		initiateRead();
		
		//delay one second
		delayMicroseconds(1000000);
		
		// Write results to console and output.
		if (currentState == ERROR_STATE)
		{
			analyzeAndPrintResults((int *)bitsRcvd, "Error reading from the sensor\n", "interrupts");
		}
		else
		{
			analyzeAndPrintResults((int *)bitsRcvd, NULL, "interrupts");
		}
		
		++readIdx;
	}
		
	// Free the GPIO
	releaseGpio();
	
	return 0;
}

/*
 * Prepares the sensor for reading. This function sets the state machine
 * to its initial state, pulls (and keeps low) the sensor pin,
 * and sets readReady to true. It then calls the ISR directly
 * in order to let the sensor take over controlling the line.
 */
void initiateRead()
{	
	currentState = INIT_PULL_LINE_LOW;
	
	// Prepare the sensor
	// Reserve the GPIO pin
	pinMode(SENSOR_PIN_NUM, OUTPUT);
	digitalWrite(SENSOR_PIN_NUM, LOW);
	delay(20);
	
	// Let the ISR know that sensor is ready to read.
	readReady = true;
	
	sensorReadISR();	
}

/*
 * ISR for reading the sensor. There are several states which this ISR accounts for.
 */
void sensorReadISR()
{
	switch (currentState)
	{
		case INIT_PULL_LINE_LOW :
			if (readReady == true)
			{
				currentReadingBitIdx = 0;				
				microsPreviousRisingEdge = 0;				
				currentState = INPUT_JUST_ENABLED;			
				//digitalWrite(SENSOR_PIN_NUM, HIGH);
				pinMode(SENSOR_PIN_NUM, INPUT);
			}
			break;
		
		// Temporary, single-use state. It serves to handle
		// the case where the sensor pin is set to an input
		// in the INIT_PULL_LINE_LOW state, triggering an
		// unwanted interrupt.
		case INPUT_JUST_ENABLED:
			currentState = HIGH_ACK;
			break;
			
		// Entered at the beginning of the high response
		case HIGH_ACK :
			currentState = BIT_READ_RISING;
			break;

		case BIT_READ_RISING:
			{
			microsCurrentRisingEdge = micros();
			// If this is the first bit, we have no reference for a differential reading.
			// The read time has been set, so break and wait for the next rising edge
			// to determine how long this cycle was high.


			
			// Get the time when this edge occurred
						
			// Get the amount of uS the bit-determining pulse was high.
			// Subtract 50us, the pre-bit delay.
			prevBitHighTime = microsCurrentRisingEdge - microsPreviousRisingEdge;
			microsPreviousRisingEdge = microsCurrentRisingEdge;
			
			if (currentReadingBitIdx == 0)
			{
				++currentReadingBitIdx;
				break;
			}
						
			// Distinguish the bit using prevBitHighTime
			// Don't forget there might be error if the time is too large.
			if(prevBitHighTime <= 99)
			{
				bitsRcvd[currentReadingBitIdx - 1] = 0;
			}
			if(prevBitHighTime >= 100)
			{
				bitsRcvd[currentReadingBitIdx - 1] = 1;
			}

			// Account for the this bit's high time.
			measuredBitHighTime[currentReadingBitIdx - 1] = prevBitHighTime;	
				
			++currentReadingBitIdx;
			
			// The "-1" accounts for a 0-initial value for the idx
			// At this point, the last bit time has to be read, so delay
			// 40, then poll the input to see if it's high.
			// If it is, then the current bit must be a 1.
			// The side effect to this is that if this bit is 
			// held too long, we won't be able to tell.
			if (currentReadingBitIdx >= 41)
			{
				delay(40);
				currentState = READ_COMPLETE;
				
				if (digitalRead(SENSOR_PIN_NUM) == LOW)
				{
					bitsRcvd[currentReadingBitIdx - 1] = 0;
				}
				else
				{
					bitsRcvd[currentReadingBitIdx - 1] = 1;
				}
			}
			
			break;
			}
			
		default :
			break;
	}
}


/*
 * Initializes the GPIO pin to high
 */
void setupGpio()
{
	// Call setup
	
	// Reserve the GPIO pin
	
	// Set the line high by default
	wiringPiSetup();	

	pullUpDnControl(SENSOR_PIN_NUM, PUD_UP);
}

// Releases the GPIO reservation.
void releaseGpio()
{

}

/*
 * Returns the 8-bit integer whose first bit is located at
 * at bits_rcvd[offset]
 */
int arrAndOffsetToInt(int * bits_rcvd, int offset)
{
	int val = 0;
	int bit_idx = 0;
	for (; bit_idx < BITS_PER_BYTE; ++bit_idx)
	{
		val |= bits_rcvd[offset + 7 - bit_idx] << bit_idx;
	}
	return val;
}

/*
 * Generates a checksum from the humidity and temp readings.
 */
int generateChecksum(uint8_t* bits_rcvd)
{
	// Use uint8_t variables to ensure that the result of each addition
	// is only eight bits.
	uint8_t checksum;
	memset(dht11_dat, 0, 5);
	for(int sum_idx= 0; sum_idx < 40; ++sum_idx)
	{
		int byte_num = (sum_idx/8);
		int bit_num = 7-(sum_idx % 8);

//		printf("Bit %d value: %d \n\r",sum_idx, bits_rcvd[sum_idx]);
		printf("bit %d tume: %d \n\r", sum_idx, measuredBitHighTime[sum_idx]);

		dht11_dat[byte_num] |= (bits_rcvd[sum_idx]<<bit_num);

	}

	checksum = ((dht11_dat[0]+dht11_dat[1]+dht11_dat[2]+dht11_dat[3])&0xFF);
	return checksum;
}

/*
 * Processes the data read, and writes to a file in the event that
 * the reading was successful.
 */
void analyzeAndPrintResults(int * bitsRcvd, const char * errorString, const char * sensorInteractionMode)
{
	int temp_int = 0;
	int temp_dec = 0;
	int humid_int = 0;
	int humid_dec = 0;
	
	if (errorString != NULL)
	{
		printf("%s", errorString);
	}

	char * timeAsString = getTimeAsString();
	
	if (timeAsString != NULL)
	{
		printf("Time of reading: %s", timeAsString);
	}
	
	// Check checksum
	uint8_t checksum_generated = generateChecksum(bitsRcvd);
	uint8_t checksum_read = dht11_dat[4];

	if(checksum_read == checksum_generated)
		readSuccessful = true;	
	humid_int = dht11_dat[0];
	humid_dec = dht11_dat[1];
	temp_int = dht11_dat[2];
	temp_dec  = dht11_dat[3];
	
	// Print the results
	if(checksum_generated == checksum_read)
	{
		printf("Temp: %d.%d\n", temp_int, temp_dec);
		printf("Humidity: %d.%d\n", humid_int, humid_dec);
	}
		
	for(int i=0; i<5; i++)
		{
			printf("Byte %d value %X \n\r", i, dht11_dat[i]);
	
	
		}
		
	// Write the results to a file
	writeResultsToFile(temp_int, temp_dec, humid_int, humid_dec, checksum_read, checksum_generated, sensorInteractionMode, timeAsString, errorString);
}

/*
 * Write the temperature, humidity, and the sensor communcation type to
 * a file. The filename is the constant OUTPUT_FILENAME. It appends to
 * the file rather than overwrites it (if it exists already). The file
 * will be searched for in the current working directory.
 */
void writeResultsToFile(int temp_int, int temp_dec, 
						int humid_int, int humid_dec,
						int checksumRead, int checksumGen,
						const char * sensorInteractionMode,
						const char * timeAsString,
						const char * errorString)
{
	// Open the file
	FILE * outFp = fopen(OUTPUT_FILENAME, "a");
	
	if (outFp == NULL)
	{
		printf("\nERROR WRITING DATA TO FILE!\n");
	}
	
	fprintf(outFp, "\n\n\nRESULTS OF READING VIA: %s\n", sensorInteractionMode);
	
	if (errorString != NULL)
	{
		fprintf(outFp, "%s", errorString);
	}
	
	if (timeAsString == NULL)
	{
		fprintf(outFp, "ERROR RETRIEVING TIME\n");
	}
	else
	{
		fprintf(outFp, "TIME OF MEASUREMENT: %s", timeAsString);
	}
	
	// Print the results
	fprintf(outFp, "Temp: %d.%d\n", temp_int, temp_dec);
	fprintf(outFp, "Humidity: %d.%d\n", humid_int, humid_dec);
	fprintf(outFp, "Checksum read: %d\n", checksumRead);
	fprintf(outFp, "Checksum calc'd: %d\n", checksumGen);
	fprintf(outFp, "Checksum valid: %s\n", checksumRead == checksumGen ? "true" : "false");
	
	if(fclose(outFp) != 0)
	{
		printf("\nERROR CLOSING FILE!\n");
	}
}

/*
 * Gets the current time/date as string.
 */
char * getTimeAsString()
{
	// Get the time and write it to the file.
	time_t currentTime;
	char * timeAsString;
	currentTime = time(NULL);
	if (currentTime == ((time_t)-1))
	{
		printf("ERROR ACCESSING TIME!\n");
	}
	
	timeAsString = ctime(&currentTime);
	if (timeAsString == NULL)
	{
		printf("ERROR CONVERTING TIME TO STRING!\n");
	}
	
	return timeAsString;
}

/*
 * piHiPri:
 *	Attempt to set a high priority schedulling for the running program
 */

int piHiPri (const int pri)
{
  struct sched_param sched ;

  memset (&sched, 0, sizeof(sched)) ;

  if (pri > sched_get_priority_max (SCHED_RR))
    sched.sched_priority = sched_get_priority_max (SCHED_RR) ;
  else
    sched.sched_priority = pri ;

  return sched_setscheduler (0, SCHED_RR, &sched) ;
}
