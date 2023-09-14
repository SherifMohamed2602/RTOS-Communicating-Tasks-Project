/*Standard Library Includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Kernel Includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "time.h"

/*
 *This section defines constants used throughout the program.
 *These constants are used to specify limits, thresholds, and other
 *values that are used in multiple parts of the program.
 */
#define CCM_RAM __attribute__((section(".ccmram")))	 		                  // Defines a variable to be stored in CCM RAM
#define STACK_SIZE 1024						       	                 	     // Used as the third argument in xTaskCreate() to specify the stack size of a task
#define QUEUE_SIZE 3					             	                    // Used as the first argument in xQueueCreate() to specify the maximum number of items in a queue
#define QUEUE_ITEM_SIZE sizeof(char*)	         	                       // Used as the second argument in xQueueCreate() to specify the size of each item in a queue
#define MESSAGE_COUNT 1000			         	                          // Used to specify the number of messages to be sent and received by the program
#define T_RECEIVER 100							                         // Used to specify the period of the receiver timer in milliseconds
#define T_Sender1 getRandomPeriod(minPeriod, maxPeriod, senderTimer1)   // Used to specify the random period of the sender timer 1
#define T_Sender2 getRandomPeriod(minPeriod, maxPeriod, senderTimer2)  // Used to specify the random period of the sender timer 2
#define T_Sender3 getRandomPeriod(minPeriod, maxPeriod, senderTimer3) // Used to specify the random period of the sender timer 3
#define MAX_BUF_SIZE 5000								             // Used as the buffer size for snprintf() in the printAll() function
#define MAX_MESSAGE_LENGTH sizeof(char*) *30              		    // Used to specify the maximum length of a message in the sender and receiver task
#define MAX_TIME_LENGTH sizeof(char*) *20			               // Used to specify the maximum length of a timestamp in the sender and receiver task
#define RECEIVER_PRIORITY 3					   	                  // Used to set the priority of the receiver task
#define PRIORITY_HIGH 2						  	                 // Used to set the priority of high-priority tasks
#define PRIORITY_LOW 1						 	                // Used to set the priority of low-priority tasks
#define ITERATION_COUNT 5						               // Used to specify the number of times the sender task sends a message

/*
 *Function Prototypes:
 *This section contains declarations of functions that are used in the program but are defined
 *elsewhere in the code. By declaring the functions here, the program can use them before their
 *actual definitions are provided later in the code.
 */
void startTimers();
void createTasks(QueueHandle_t);
void createSemaphores();
void resetPeriods(void);
void systemInit(void*);
int  getRandomPeriod(int, int, void *);
void timerCallback(TimerHandle_t);
void createTimers(QueueHandle_t);
void reset(void*);
void senderTask1(void*);
void senderTask2(void*);
void senderTask3(void*);
void receiverTask(void*);
void printAll(void);

/*
 *Semaphore, Timer and Queue Handles:
 *These variables hold handles to the semaphores, timers and Queues used in the program.
 */
SemaphoreHandle_t senderSemaphore1;
SemaphoreHandle_t senderSemaphore2;
SemaphoreHandle_t senderSemaphore3;
SemaphoreHandle_t receiverSemaphore;

QueueHandle_t queue;

TimerHandle_t senderTimer1;
TimerHandle_t senderTimer2;
TimerHandle_t senderTimer3;
TimerHandle_t receiverTimer;

/*
 *Timer Started Flags:
 *These variables indicate whether the corresponding timers have been started.
 */

BaseType_t senderTimer1Started;
BaseType_t senderTimer2Started;
BaseType_t senderTimer3Started;
BaseType_t receiverTimerStarted;

/*
 *Period Boundaries:
 *These arrays define the minimum and maximum values of the period for a task.
 *There are 6 values in each array, corresponding to the 6 different priority levels
 *used in the program.
 */
int minPeriodBoundries[6] = { 50,
	80,
	110,
	140,
	170,
	200
};

int maxPeriodBoundries[6] = { 150,
	200,
	250,
	300,
	350,
	400
};

/*
 *Task Parameters and Counters:
 *These variables are used to store the parameters and counters for the different tasks
 *in the program. They include the minimum and maximum period for a task, the number of
 *iterations, and various counters that keep track of the number of messages sent and received
 *by each task.
 */

int minPeriod;
int maxPeriod;
int iterations;

int sentCount1;
int sentCount2;
int sentCount3;
int blockedCount1;
int blockedCount2;
int blockedCount3;
int receivedCount;
int sumPeriod1;
int sumPeriod2;
int sumPeriod3;

void
startTimers(){
	// Start the sender and receiver timers and check that they started successfully
	if (xTimerStart(senderTimer1, 0) != pdPASS || xTimerStart(senderTimer2, 0) != pdPASS ||
		xTimerStart(senderTimer3, 0) != pdPASS || xTimerStart(receiverTimer, 0) != pdPASS)
	{
		printf("Error starting timers\n");
	}
}

/*
 *create Tasks:
 *This function creates the sender and receiver tasks. The sender tasks are created with
 *different priorities, with sender task 1 having the highest priority and sender tasks 2 and 3
 *having lower priorities. The receiver task has a separate higher priority.
 */

void
createTasks(QueueHandle_t queue){

	// Create the sender and receiver tasks and check successful creation
	if (xTaskCreate(senderTask1, "Sender Task 1", STACK_SIZE, (void*) queue, PRIORITY_HIGH, NULL) != pdPASS)
	{
		printf("Error creating sender task 1\n");

	}

	if (xTaskCreate(senderTask2, "Sender Task 2", STACK_SIZE, (void*) queue, PRIORITY_LOW, NULL) != pdPASS)
	{
		printf("Error creating sender task 2\n");

	}

	if (xTaskCreate(senderTask3, "Sender Task 3", STACK_SIZE, (void*) queue, PRIORITY_LOW, NULL) != pdPASS)
	{
		printf("Error creating sender task 3\n");

	}

	if (xTaskCreate(receiverTask, "Receiver Task", STACK_SIZE, (void*) queue, RECEIVER_PRIORITY, NULL) != pdPASS)
	{
		printf("Error creating receiver task\n");

	}
}

/*
 *create Semaphores:
 *This function creates the semaphores needed to synchronize the task communication
 */
void
createSemaphores(){
	// Create the semaphores
	senderSemaphore1 = xSemaphoreCreateBinary();
	senderSemaphore2 = xSemaphoreCreateBinary();
	senderSemaphore3 = xSemaphoreCreateBinary();
	receiverSemaphore = xSemaphoreCreateBinary();
}

/*
 *Reset Periods:
 *This function resets the minimum and maximum periods for a task. It also increments the
 *iteration count and checks if the maximum number of iterations has been reached. If so,
 *it stops the program by deleting the timers and ending the scheduler.
 */

void
resetPeriods()
{

	iterations++;
	printf("reseting periods after iteration number: %d\n\n\n", iterations);
	if (iterations > ITERATION_COUNT)
	{
		printf("Game Over\n");
		xTimerDelete(senderTimer1, 0);
		xTimerDelete(senderTimer2, 0);
		xTimerDelete(senderTimer3, 0);
		xTimerDelete(receiverTimer, 0);
		exit(0);
	}

	// Reset the minimum and maximum period
	minPeriod = minPeriodBoundries[iterations];
	maxPeriod = maxPeriodBoundries[iterations];

	//changing the periods of sender timers with new boundaries

	if (xTimerChangePeriod(senderTimer1, pdMS_TO_TICKS(T_Sender1), 0) != pdPASS)
	{
		printf("Error changing period of senderTimer1\n");
	}
	if (xTimerChangePeriod(senderTimer2, pdMS_TO_TICKS(T_Sender2), 0) != pdPASS)
	{
		printf("Error changing period of senderTimer2\n");
	}
	if (xTimerChangePeriod(senderTimer3, pdMS_TO_TICKS(T_Sender3), 0) != pdPASS)
	{
		printf("Error changing period of senderTimer3\n");
	}

}

/*
 *System Initialization:
 *This function initializes the counters and periods used in the program. It also seeds
 *the random number generator with the current time.
 */

void
systemInit(QueueHandle_t qpointer)
{
	// Initialize the counters
	sentCount1 = 0;
	sentCount2 = 0;
	sentCount3 = 0;
	blockedCount1 = 0;
	blockedCount2 = 0;
	blockedCount3 = 0;
	receivedCount = 0;
	sumPeriod1 = 0;
	sumPeriod2 = 0;
	sumPeriod3 = 0;
	// Initialize Periods
	iterations = 0;
	minPeriod = minPeriodBoundries[0];
	maxPeriod = maxPeriodBoundries[0];

	//Check that Queue was reseted successfully
	if (xQueueReset(qpointer) == pdFALSE)
	{
		printf("error queue not reseted \n");
	}

	// Seed the random number generator with the current time
	srand((unsigned int) time(NULL));
}

/*
 *Get Random Period:
 *This function generates a random period between the minimum and maximum periods for a task.
 *It uses the rand() function to generate a random number, and then scales the number to
 *fit within the range of the minimum and maximum periods.
 */
int
getRandomPeriod(int minPeriod, int maxPeriod, TimerHandle_t xTimer )
{
	int period = rand() % (maxPeriod - minPeriod + 1) + minPeriod; // For sender tasks random periods

    // Determine which sum of periods to add to based on the timer handle
    if (xTimer == senderTimer1)
		{
			sumPeriod1 += period;
		}
		else if (xTimer == senderTimer2)
		{
			sumPeriod2 += period;
		}
		else if (xTimer == senderTimer3)
		{
			sumPeriod3 += period;
		}

    return period;
}

/*
 *Timer Callback:
 *This function is called when a timer expires. It releases the semaphore that the
 *corresponding task is waiting on, allowing the task to proceed. If the number of messages received equals MESSAGE_COUNT,
 *the reset() function is called to reset the queue and statistics.
 */
void timerCallback(TimerHandle_t xTimer)
{
	QueueHandle_t queue = (QueueHandle_t) pvTimerGetTimerID(xTimer);


	if (xTimer == receiverTimer)
	{

		// Release the semaphore that the receiver task is waiting on
		xSemaphoreGive(receiverSemaphore);

		// Check if MESSAGE_COUNT equals messages received
	if (receivedCount == MESSAGE_COUNT)
	{
		reset(queue);
	}

	}
	else
	{
		// Determine which sender semaphore to release based on the timer handle
		SemaphoreHandle_t senderSemaphore = NULL;
		if (xTimer == senderTimer1)
		{
			senderSemaphore = senderSemaphore1;
		}
		else if (xTimer == senderTimer2)
		{
			senderSemaphore = senderSemaphore2;
		}
		else if (xTimer == senderTimer3)
		{
			senderSemaphore = senderSemaphore3;
		}

		if (senderSemaphore != NULL)
		{
			// Release the semaphore that the sender task is waiting on
			xSemaphoreGive(senderSemaphore);
		}
	}

}

/*
 *Create Sender Timers:
 *This function creates the sender timers with a period set to a random value between the
 *minimum and maximum periods. It uses the getRandomPeriod() function to generate the
 *random period value, and the xTimerCreate() function to create the timers.
 *it also creates the receiver timer with a period set to the constant T_RECEIVER.
 */
void
createTimers(QueueHandle_t queue)
{
	// Create the sender timers
	senderTimer1 =
		xTimerCreate("Sender Timer 1", pdMS_TO_TICKS(T_Sender1), pdTRUE, (void*)queue, (TimerCallbackFunction_t) timerCallback);
	senderTimer2 =
		xTimerCreate("Sender Timer 2", pdMS_TO_TICKS(T_Sender2), pdTRUE, (void*)queue, (TimerCallbackFunction_t) timerCallback);
	senderTimer3 =
		xTimerCreate("Sender Timer 3", pdMS_TO_TICKS(T_Sender3), pdTRUE, (void*)queue, (TimerCallbackFunction_t) timerCallback);
	// Create the receiver timer
	receiverTimer =
		xTimerCreate("Receiver Timer", pdMS_TO_TICKS(T_RECEIVER), pdTRUE, (void*)queue, (TimerCallbackFunction_t) timerCallback);
}

/*
 *Reset:
 *This function resets the queue and statistics used in the program. It prints all statistics
 *to the console, resets the counters, and then resets the queue. Finally, it calls the
 *resetPeriods() function to reset the periods used in the program.
 */
void reset(QueueHandle_t qpointer)
{
	printAll();

	//reset statistics
	sentCount1 = 0;
	sentCount2 = 0;
	sentCount3 = 0;
	blockedCount1 = 0;
	blockedCount2 = 0;
	blockedCount3 = 0;
	receivedCount = 0;
	sumPeriod1 = 0;
	sumPeriod2 = 0;
	sumPeriod3 = 0;

	//Check that Queue was reseted successfully
	if (xQueueReset(qpointer) == pdFALSE)
	{
		printf("error queue not reseted \n");
	}

	resetPeriods();

}

/*
 *Sender Tasks:
 *These tasks are responsible for sending messages to the queue at random intervals.
 *Each task waits for its associated sender timer semaphore, generates a message that
 *includes the current tick count, and attempts to send the message to the queue. If the
 *message is successfully sent, the sentCountX counter for the corresponding task is
 *incremented. If the message cannot be sent, the blockedCountX counter is incremented.
 *The message is then freed, and the sender timer period is updated with a new random value.
 */

void senderTask1(void *pvParameters)
{
	QueueHandle_t queue = (QueueHandle_t) pvParameters;

	char *prefix = "Time is ";
	TickType_t xTimeNow;
	char *time = pvPortMalloc(MAX_TIME_LENGTH);
	char *message = pvPortMalloc(MAX_MESSAGE_LENGTH);

	if (time == NULL || message == NULL)
	{
		printf("Error allocating memory for message\n");
		vTaskDelete(NULL);
	}

	while (1)
	{
		// Wait for the sender timer semaphore
		if (xSemaphoreTake(senderSemaphore1, portMAX_DELAY) != pdTRUE)
		{
			printf("Error taking senderSemaphore1\n");
			continue;
		}

		xTimeNow = xTaskGetTickCount();
		sprintf(time, "%lu", xTimeNow);
		strcpy(message, prefix);
		strcat(message, time);
		// Try to send the message to the queue
		if (xQueueSend(queue, &message, 0) == pdTRUE)
		{
			// Message was successfully sent
			sentCount1++;
		}
		else
		{
			// Message could not be sent
			blockedCount1++;
		}

		if (xTimerChangePeriod(senderTimer1, pdMS_TO_TICKS(T_Sender1), 0) != pdPASS)
		{
			printf("Error changing period of senderTimer1\n");
		}
	}

	vPortFree(time);
	vPortFree(message);
}

void senderTask2(void *pvParameters)
{
	QueueHandle_t queue = (QueueHandle_t) pvParameters;
	char *prefix = "Time is ";
	TickType_t xTimeNow;
	char *time = pvPortMalloc(MAX_TIME_LENGTH);
	char *message = pvPortMalloc(MAX_MESSAGE_LENGTH);

	if (time == NULL || message == NULL)
	{
		printf("Error allocating memory for message\n");
		vTaskDelete(NULL);
	}

	while (1)
	{
		// Wait for the sender timer semaphore
		if (xSemaphoreTake(senderSemaphore2, portMAX_DELAY) != pdTRUE)
		{
			printf("Error taking senderSemaphore2\n");
			continue;
		}

		xTimeNow = xTaskGetTickCount();
		sprintf(time, "%lu", xTimeNow);
		strcpy(message, prefix);
		strcat(message, time);
		// Try to send the message to the queue
		if (xQueueSend(queue, &message, 0) == pdTRUE)
		{
			// Message was successfully sent
			sentCount2++;
		}
		else
		{
			// Message could not be sent
			blockedCount2++;
		}

		if (xTimerChangePeriod(senderTimer2, pdMS_TO_TICKS(T_Sender2), 0) != pdPASS)
		{
			printf("Error changing period of senderTimer2\n");
		}
	}

	vPortFree(time);
	vPortFree(message);
}

void senderTask3(void *pvParameters)
{
	QueueHandle_t queue = (QueueHandle_t) pvParameters;
	char *prefix = "Time is ";
	TickType_t xTimeNow;
	char *time = pvPortMalloc(MAX_TIME_LENGTH);
	char *message = pvPortMalloc(MAX_MESSAGE_LENGTH);

	if (time == NULL || message == NULL)
	{
		printf("Error allocating memory for message\n");
		vTaskDelete(NULL);
	}

	while (1)
	{

		// Wait for the sender timer semaphore
		if (xSemaphoreTake(senderSemaphore3, portMAX_DELAY) != pdTRUE)
		{
			printf("Error taking senderSemaphore3\n");
			continue;
		}

		xTimeNow = xTaskGetTickCount();
		sprintf(time, "%lu", xTimeNow);
		strcpy(message, prefix);
		strcat(message, time);

		// Try to send the message to the queue
		if (xQueueSend(queue, &message, 0) == pdTRUE)
		{
			// Message was successfully sent
			sentCount3++;
		}
		else
		{
			// Message could not be sent
			blockedCount3++;
		}

		if (xTimerChangePeriod(senderTimer3, pdMS_TO_TICKS(T_Sender3), 0) != pdPASS)
		{
			printf("Error changing period of senderTimer3\n");
		}

	}
	vPortFree(time);
	vPortFree(message);
}

/*
 *Receiver Task:
 *This task is responsible for receiving messages from the queue and printing them to the console.
 *It waits for the receiver timer semaphore, attempts to receive a message from the queue, and
 *if a message is received, it increments the receivedCount counter and prints the message to
 *the console.
 */
void receiverTask(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t) pvParameters;
    char *receivedMessage = pvPortMalloc(MAX_MESSAGE_LENGTH);
    configASSERT(receivedMessage != NULL);

    while (1)
    {
        // Wait for the receiver timer semaphore
        if (xSemaphoreTake(receiverSemaphore, portMAX_DELAY) != pdTRUE)
        {
            printf("Error taking receiverSemaphore\n");
            continue;
        }

        // Receive a message from the queue
        xQueueReceive(queue, &receivedMessage, portMAX_DELAY);

        // Message was successfully received
        receivedCount++;
        printf("receiverTask: Received message: %s\n", receivedMessage);
    }

    vPortFree(receivedMessage);
}
/*
 *Print All:
 *This function is responsible for printing statistics to the console. It calculates the total
 *number of messages sent and blocked, and outputs detailed statistics for each sender task.
 *If any statistics variables are not initialized(negative), it outputs an error message.
 */

void
printAll()
{
	char buf[MAX_BUF_SIZE];
	int totalSent = sentCount1 + sentCount2 + sentCount3;
	int totalBlocked = blockedCount1 + blockedCount2 + blockedCount3;
	int totalSumPeriods = sumPeriod1 + sumPeriod2 + sumPeriod3;
	int avgPeriod1 = (sentCount1 + blockedCount1) > 0 ? sumPeriod1 / (sentCount1 + blockedCount1) : 0;
	int avgPeriod2 = (sentCount2 + blockedCount2) > 0 ? sumPeriod2 / (sentCount2 + blockedCount2) : 0;
	int avgPeriod3 = (sentCount3 + blockedCount3) > 0 ? sumPeriod3 / (sentCount3 + blockedCount3) : 0;

	// Print error message if any statistics variables are negative
	if (sentCount1 < 0 || sentCount2 < 0 || sentCount3 < 0 || blockedCount1 < 0 || blockedCount2 < 0 || blockedCount3 < 0)
	{
	    printf("Error: statistics variables are negative\n");
	    return;
	}


	snprintf(buf, MAX_BUF_SIZE, "Total number of successfully sent messages is %d\n"
	                             "Total number of blocked messages is %d\n"
	                             "Total number of received messages is %d\n"
	                             "Total sum of period is %d\n\n"
	                             "Task 1 Statistics (Higher priority):\n"
	                             "Number of successfully sent messages: %d\n"
	                             "Number of blocked messages: %d\n"
	                             "Sum of periods:%d\n"
	                             "Average period:%d\n\n"
	                             "Task 2 Statistics (Low priority):\n"
	                             "Number of successfully sent messages: %d\n"
	                             "Number of blocked messages: %d\n"
	                             "Sum of periods:%d\n"
	                             "Average period:%d\n\n"
	                             "Task 3 Statistics (Low priority):\n"
	                             "Number of successfully sent messages: %d\n"
	                             "Number of blocked messages: %d\n"
	                             "Sum of periods:%d\n"
	                             "Average period:%d\n\n",
	                             totalSent, totalBlocked, receivedCount, totalSumPeriods,
	                             sentCount1, blockedCount1, sumPeriod1, avgPeriod1,
	                             sentCount2, blockedCount2, sumPeriod2, avgPeriod2,
	                             sentCount3, blockedCount3, sumPeriod3, avgPeriod3);
	puts(buf);
	fflush(stdout);
}

/*
 *Compiler Pragma Directives:
 *These pragmas modify the behavior of the GCC compiler with respect to warnings.
 *The "push" directive saves the current state of the compiler's diagnostic system,
 *allowing the code to modify the diagnostic behavior temporarily. The "ignored"
 *directives suppress warnings issued by the compiler, specifically
 *"-Wunused-parameter", "-Wmissing-declarations", "-Wunused-function", and "-Wreturn-type".
 *The purpose of these directives is to suppress certain warnings that may be generated
 *by the compiler during compilation. This can be useful in cases where the developer is
 *aware of the potential warnings and has decided to ignore them intentionally. The
 *"pop" directive restores the previous diagnostic state of the compiler, ensuring that
 *any warnings suppressed by the "ignored" directives are only ignored within the scope
 *of the function.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wreturn-type"

/*
 *Main Function:
 *This function is responsible for initializing the system and hardware peripherals,
 *creating the semaphores and queue, and creating the sender and receiver tasks and timers.
 *If any of these operations fail, an error message is printed and the function returns 1.
 *If all operations succeed, the scheduler is started and the function returns 0.
 *
 *The sender and receiver tasks are created.
 *The sender and receiver timers are created and started, allowing messages to be sent and received
 *periodically.
 */

int main()
{
    // Create the queue and check that it was created successfully
    QueueHandle_t queue = xQueueCreate(QUEUE_SIZE, QUEUE_ITEM_SIZE);
    if (queue == NULL)
    {
        printf("Error creating queue\n");
        return 1;
    }

    // Initialize the system and hardware peripherals
    systemInit(queue);

    // Create the semaphore and check successful creation
    createSemaphores();
    if (senderSemaphore1 == NULL || senderSemaphore2 == NULL || senderSemaphore3 == NULL || receiverSemaphore == NULL)
    {
        printf("Error creating semaphores\n");
        return 1;
    }

    // Create sender and receiver tasks, and timers
    createTasks(queue);
    createTimers(queue);

    // Check successful creation of timers
    if (senderTimer1 == NULL || senderTimer2 == NULL || senderTimer3 == NULL || receiverTimer == NULL)
    {
        printf("Error creating timers\n");
        return 1;
    }

    // Start the timers and scheduler
    startTimers();
    vTaskStartScheduler();

    return 0;
}

# pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

void vApplicationMallocFailedHook(void)
{
	/*Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for (;;);
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void) pcTaskName;
	(void) pxTask;

	/*Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for (;;);
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	volatile size_t xFreeStackSpace;

	/*This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if (xFreeStackSpace > 100)
	{
		/*By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
	/*Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/*Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/*Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/*configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
