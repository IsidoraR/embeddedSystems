#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sched.h>

#define DHTPIN 7
#define TESTPIN 0



void pollDHTpin(void);
void set_default_priority(void);
void set_max_priority(void);

void set_max_priority(void)
{
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &sched);
}

void set_default_priority(void)
{
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	sched.sched_priority = 0;
	sched_setscheduler(0, SCHED_OTHER, &sched);
}
void pollDHTpin()
{
		uint32_t start_time;
		uint32_t time_difference;
		struct timespec gettime_now;
		uint32_t rising_time = 0;
		uint32_t falling_time = 0;
		uint32_t pulseWidth_arr[41];
		pinMode(DHTPIN, OUTPUT);
		digitalWrite(DHTPIN, LOW);
		delay(20);
		pinMode(DHTPIN, INPUT);
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		start_time = gettime_now.tv_nsec;
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = (gettime_now.tv_nsec-start_time);
		int pin_state;
		int prev_state = digitalRead(DHTPIN);
		uint8_t index = 0;
		//set_max_priority();
		while((time_difference < 1E9) && (index < 41))
		{
		//	printf("time difference: %d \n\r", time_difference);
			clock_gettime(CLOCK_REALTIME, &gettime_now);
			pin_state = digitalRead(DHTPIN);
			time_difference = gettime_now.tv_nsec - start_time;
			digitalWrite(TESTPIN, pin_state);
			if(pin_state != prev_state)
			{
				if(pin_state == 1)
				{
					rising_time = gettime_now.tv_nsec;
				}	
				else
				{
					falling_time = gettime_now.tv_nsec;
					pulseWidth_arr[index] = (falling_time - rising_time);
					index++;
				}
			}
		}
		uint32_t pulseSum;
		uint16_t avgPulse;
		uint8_t received_bytes[5]= {0, 0, 0, 0, 0};
		for(int i=1; i<41; i++)
		{
			pulseSum = pulseSum + pulseWidth_arr[i];	
		}
		avgPulse = (pulseSum/40);
		for(int i=0; i<40; i++)
		{
			if(pulseWidth_arr[i] < avgPulse)
			{
				uint8_t byte_n = (i/8);
				uint8_t bit_n = (i%8);
				received_bytes[byte_n] |= (1<<bit_n);
			}
		}
		for(int i=0; i<5; i++)
		{
			//printf("Byte %d, value: %X \n\r", i, received_bytes[i]);
		}
		printf( "Humidity = %d.%d %% Temperature = %d.%d C \n",received_bytes[0], received_bytes[1], received_bytes[2], received_bytes[3]); 
		//printf("Done \n\r");
	//set_default_priority();

}

//void process_data()
//{

//}

int main()
{
	wiringPiSetup();
	pinMode(TESTPIN, OUTPUT);
	for(int i=0; i<10; i++)
	{	
		pollDHTpin();
		//
		/*
		digitalWrite(TESTPIN, LOW);
		delay(1000);
		digitalWrite(TESTPIN, HIGH);
		i
		*/
		delay(1000);
		//process_data();


			
	}	
}
