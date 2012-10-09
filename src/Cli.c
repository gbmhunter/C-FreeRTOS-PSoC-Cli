//!
//! @file 		Cli.c
//! @author 	Geoffrey Hunter <gbmhunter@gmail.com> (www.cladlab.com)
//! @date 		12/09/2012
//! @brief 		Implements the behaviour of comms commands, sends commands via queues to other tasks.
//!				Talks to the FreeRTOS CLI (FreeRTOS_CLI.c)
//! @details
//!		<b>Last Modified:			</b> 09/10/2012					\n
//!		<b>Version:					</b> v1.0.0						\n
//!		<b>Company:					</b> CladLabs					\n
//!		<b>Project:					</b> Free Code Examples			\n
//!		<b>Language:				</b> C							\n
//!		<b>Compiler:				</b> GCC						\n
//! 	<b>uC Model:				</b> PSoC5						\n
//!		<b>Computer Architecture:	</b> ARM						\n
//! 	<b>Operating System:		</b> FreeRTOS v7.2.0			\n
//!		<b>License:					</b> GPLv3						\n
//!		<b>Documentation Format:	</b> Doxygen					\n
//!
//!		Some of the code adapted from FreeRTOS-CLI examples.
//!

//===============================================================================================//
//========================================= INCLUDES ============================================//
//===============================================================================================//

// Standard PSoC includes
#include <device.h>
#include <stdio.h>
#include <stdlib.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "./FreeRTOS/FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"

// User includes
#include "PublicDefinesAndTypeDefs.h"
#include "Config.h"
#include "Cli.h"
#include "UartComms.h"
#include "UartDebug.h"
#include "Lights.h"

//===============================================================================================//
//================================== PRECOMPILER CHECKS =========================================//
//===============================================================================================//

#ifndef configENABLE_TASK_COMMS_INTERFACE
	#error Please define the switch configENABLE_TASK_COMMS_INTERFACE
#endif

#ifndef configPRINT_DEBUG_COMMS_INTERFACE
	#error Please define the switch configPRINT_DEBUG_COMMS_INTERFACE
#endif

//===============================================================================================//
//================================== PRIVATE FUNCTION PROTOTYPES ================================//
//===============================================================================================//

void Cli_Task(void *pvParameters);

static portBASE_TYPE prvCmd1(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvCmd2(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);

//===============================================================================================//
//============================= PRIVATE VARIABLES/STRUCTURES ====================================//
//===============================================================================================//

static xTaskHandle _cliTaskHandle = 0;

// CLI Commands

//! Command 1. Has no parameters.
static const CLI_Command_Definition_t xCommand1 =
{
    (const int8_t*)"cmd1",												//!< Text to type in command-line to exectue command
    (const int8_t*)"cmd1: Has no parameters\r\n",						//!< Text to return on help
    &prvCmd1,															//!< Function to run when command is typed
    0																	//!< Expected number of parameters
};

//! Command 2. Has one number parameter.
static const CLI_Command_Definition_t xCommand2 =
{
    (const int8_t*)"cmd2",												//!< Text to type in command-line to exectue command
    (const int8_t*)"cmd2 : Has one number parameter\r\n",				//!< Text to return on help
    &prvCmd2,															//!< Function to run when command is typed
    1																	//!< Expected number of parameters (speed)
};

//! Command 3. Has one text parameter
static const CLI_Command_Definition_t xCmd3 =
{
    (const int8_t*)"cmd3",												//!< Text to type in command-line to exectue command
    (const int8_t*)"cmd3 : Has one text parameter\r\n",					//!< Text to return on help
    &prvCmd3,															//!< Function to run when command is typed
    1																	//!< Expected number of parameters (speed)
};

//===============================================================================================//
//===================================== PUBLIC FUNCTIONS ========================================//
//===============================================================================================//

//! @brief		Initialises comms interface
//! @details	Registers the CLI commands with FreeRTOS_CLI
//! @note 		Call before starting scheduler. Command become active as soon as they are
//!				registered.
//! @public
void Cli_Start(uint32 cliTaskStackSize, uint8 cliTaskPriority)
{
	// Register commands for CLI
	FreeRTOS_CLIRegisterCommand(&xCmd1);
	FreeRTOS_CLIRegisterCommand(&xCmd2);
	FreeRTOS_CLIRegisterCommand(&xCmd3);
	
	#if(configENABLE_TASK_COMMS_INTERFACE == 1)
		// Create the main task
		xTaskCreate(	&Cli_Task,
						(signed portCHAR *) "CLI Task",
						cliTaskStackSize,
						NULL,
						cliTaskPriority,
						&_cliTaskHandle);
	#endif
}

//! @brief		Implements the bahaviour of the 'cmd1' command
//! @details	Sends the CMD_ON command word to a task through the command queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvCmd1(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	portBASE_TYPE xResult;

	// Create command variable and input the command word 'CMD_1'
	commandStruct_t commandStruct;
	commandStruct.commandWord = CMD_1;

    // Send command to the task
    xResult = xQueueSendToBack(_commandQueue, &commandStruct , configMAX_QUEUE_WAIT_TIME_MS_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Command 1 sent.\r\n\r\n" );
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command 1.\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}


//! @brief		Implements the bahaviour of the 'cmd2' command
//! @details	Sends the CMD_2 command word and value to a task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvCmd2(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	const int8_t *pcParameter1;
	portBASE_TYPE xParameter1StringLength; 
	portBASE_TYPE xResult;

    // Obtain the name of the source file, and the length of its name, from
    // the command string. The name of the source file is the first parameter.
    pcParameter1 = FreeRTOS_CLIGetParameter
        ((const int8_t*)pcCommandString, 			// The command string itself.
        (unsigned portBASE_TYPE)1,					// Return the first parameter.
        (portBASE_TYPE*)&xParameter1StringLength   	// Store the parameter string length.
        );
	
	
	// Create command variable and input the command word 'CMD_2'
	commandStruct_t commandStruct;
	commandStruct.commandWord = CMD_2;
	
	float number = atof((char*)pcParameter1);
	
	// Bounds checking
	if(number < 0.0)
	{
		UartComms_PutString("ERROR: Make sure number is above 0%.\r\n\r\n");
		return pdFALSE;
	}
	else if(dutyP > 100.0)
	{
		UartComms_PutString("ERROR: Make sure number is below 100%.\r\n\r\n");
		return pdFALSE;
	}
	else
		commandStruct.value1 = number;

    // Send command to the task
    xResult = xQueueSendToBack(_commandQueue, &commandStruct , configMAX_QUEUE_WAIT_TIME_MS_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Command 2 received. Number = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "ERROR: Could not send command CMD_2.\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the 'cmd3') command
//! @details	Sends the CMD_3 command word and values to a task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvCmd3(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	const int8_t *pcParameter1;
	portBASE_TYPE xParameter1StringLength; 
	portBASE_TYPE xResult;

    // Obtain the name of the source file, and the length of its name, from
    // the command string. The name of the source file is the first parameter.
    pcParameter1 = FreeRTOS_CLIGetParameter
        ((const int8_t*)pcCommandString, 			// The command string itself.
        (unsigned portBASE_TYPE)1,					// Return the first parameter.
        (portBASE_TYPE*)&xParameter1StringLength   	// Store the parameter string length.
        );
	
	// Create command variable and fill appropriate data
	commandStruct_t commandStruct;
	commandStruct.commandWord = CMD_3;
	
	if(!strcmp((char*)pcParameter1, "choice1"))
	{
		commandStruct.value1 = (float)CHOICE_1;
	}
	else if(!strcmp((char*)pcParameter1, "choice2"))
	{
		commandStruct.value1 = (float)CHOICE_2;
	}
	else
	{
		UartComms_PutString("ERROR: Parameter to 'cmd3' not valid.\r\n\r\n");
		return pdFALSE;
	}
	
    // Send command to the task
    xResult = xQueueSendToBack(_commandQueue, &commandStruct , configMAX_QUEUE_WAIT_TIME_MS_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "cmd3 command received. Parameter string = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "ERROR: Could not send command CMD_3 to task.\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}


//! @brief 		Task for the comms interface
//! @details	Waits for characters to be received through the comms interface and calls
//!				FreeRTOS-CLI commands to process the receieved string
//! @param		*pvParameters Void pointer (not used)
//! @note		Not thread-safe. Do not call from any task, this function is a task that
//!				is called by the FreeRTOS kernel. Public by way of function pointer.
//! @public
void Cli_Task(void *pvParameters)
{

	#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
		UartDebug_PutString("COMMS: Comms interface task has started.\r\n");
	#endif
	
	
	
	// Input/output buffers. Declared static to keep them off the stack
	static int8_t txBuffer[configCOMMS_INTERFACE_TX_BUFFER_SIZE] = {0};		//!< Output buffer. Used to build msg's to send to CLI
	static int8_t rxBuffer[configCOMMS_INTERFACE_RX_BUFFER_SIZE] = {0};		//!< Input buffer. Used to store incoming characters.
	static uint8_t rxBufferPos = 0;											//!< Input buffer position.
	
	portBASE_TYPE xMoreDataToFollow;										//!< True while still processing CLI data
	
	// Send welcome message
	UartComms_PutString(configWELCOME_MSG);

	for(;;)
	{
		char rxChar;						//!< Memory to hold incoming character
		// Wait indefinetly for byte to be received on rx queue of the comms UART (blocking)
		UartComms_GetChar(&rxChar);
		
		//! @debug
		//PinCpDebugLed1_Write(0);
		
		#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
			UartDebug_PutString("COMMS: Char received. Char = '");
			UartDebug_PutChar(rxChar);
			UartDebug_PutString("'.\r\n");
		#endif
        if( rxChar == '\r' )
        {
			// Line of text has been entered
			#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
				UartDebug_PutString("COMMS: Carriage return received, beginning input string processing.\r\n");
			#endif
			
			// Flash status light
			Lights_SendCommandToTask(CMD_SWITCH_LIGHT_FLASH_ORANGE, 0, 200);
			
            // A newline character was received, so the input command stirng is
            // complete and can be processed.  Transmit a line separator, just to 
            // make the output easier to read. */
            // FreeRTOS_write( xConsole, "\r\n", strlen( "\r\n" );

            // The command interpreter is called repeatedly until it returns 
            // pdFALSE.  See the "Implementing a command" documentation for an 
            // exaplanation of why this is.
            do
            {
                // Send the command string to the command interpreter.  Any
                // output generated by the command interpreter will be placed in the 
                // pcOutputString buffer.
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand
                (     
                    rxBuffer,   		// The command string.
                    txBuffer,  		// The output buffer.
                    sizeof(txBuffer) 	// The size of the output buffer. 
                );

                // Write the output generated by the command interpreter to the 
                // console.
				UartComms_PutString((char*)txBuffer);

            } while(xMoreDataToFollow != pdFALSE);

            // All the strings generated by the input command have been sent.
            // Processing of the command is complete.  Clear the input string ready 
            // to receive the next command.
            rxBufferPos = 0;
            memset(rxBuffer, 0x00, sizeof(rxBuffer));
			
			// Clear tx buffer also
			memset(txBuffer, 0x00, sizeof(txBuffer));
			
			#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
				UartDebug_PutString("COMMS: Processing of input buffer is complete.\r\n");
			#endif
			
        }
        else
        {
            // The if() clause performs the processing after a newline character
            // is received.  This else clause performs the processing if any other
            // character is received.
		
            //if( rxChar == '\r' )
            //{
            //    // Ignore carriage returns.
            //}
            if( rxChar == '\b' )
            {
                // Backspace was pressed.  Erase the last character in the input
                // buffer - if there are any.
                if( rxBufferPos > 0 )
                {
                    rxBufferPos--;
                    rxBuffer[rxBufferPos] = '\0';
                }
            }
            else
            {
                // A character was entered.  It was not a new line, backspace
                // or carriage return, so it is accepted as part of the input and
                // placed into the input buffer.  When a \n is entered the complete
                // string will be passed to the command interpreter.
                if( rxBufferPos < sizeof(rxBuffer))
                {
                    rxBuffer[rxBufferPos] = rxChar;
                    rxBufferPos++;
                }
				else
				{
					#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
						UartDebug_PutString("COMMS: Maximum input string length reached.\r\n");
					#endif
				}
            }
        }
	}
}

//===============================================================================================//
//==================================== PRIVATE FUNCTIONS ========================================//
//===============================================================================================//

// none

//===============================================================================================//
//============================================ ISR's ============================================//
//===============================================================================================//

// none

//===============================================================================================//
//========================================= GRAVEYARD ===========================================//
//===============================================================================================//

// none

// EOF