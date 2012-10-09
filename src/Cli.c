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

static portBASE_TYPE prvBldcOnCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvBldcOffCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvSetDutyCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvSetDirectionCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvSetControlModeCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvSyncMotorCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvSetCommutationAngleCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvEnableVelocityControlCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
static portBASE_TYPE prvSetVelocityCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);

//===============================================================================================//
//============================= PRIVATE VARIABLES/STRUCTURES ====================================//
//===============================================================================================//

static xTaskHandle _cliTaskHandle = 0;

// CLI Commands

//! On Command
static const CLI_Command_Definition_t xBldcOnCommand =
{
    (const int8_t*)"on",												//!< Text to type in command-line to exectue command
    (const int8_t*)"on : Turns the BLDC motor on\r\n",					//!< Text to return on help
    &prvBldcOnCommand,													//!< Function to run when command is typed
    0																	//!< Expected number of parameters
};

//! Off Command
static const CLI_Command_Definition_t xBldcOffCommand =
{
    (const int8_t*)"off",												//!< Text to type in command-line to exectue command
    (const int8_t*)"off : Turns the BLDC motor off\r\n",				//!< Text to return on help
    &prvBldcOffCommand,													//!< Function to run when command is typed
    0																	//!< Expected number of parameters
};

//! Set Speed Command
static const CLI_Command_Definition_t xSetDutyCommand =
{
    (const int8_t*)"sduty",												//!< Text to type in command-line to exectue command
    (const int8_t*)"sduty : Sets the duty cycle of the motor\r\n",		//!< Text to return on help
    &prvSetDutyCommand,													//!< Function to run when command is typed
    1																	//!< Expected number of parameters (speed)
};

//! Set Direction Command
static const CLI_Command_Definition_t xSetDirectionCommand =
{
    (const int8_t*)"sdir",															//!< Text to type in command-line to exectue command
    (const int8_t*)"sdir : (set-direction) Sets the direction of the motor\r\n",	//!< Text to return on help
    &prvSetDirectionCommand,														//!< Function to run when command is typed
    1																				//!< Expected number of parameters (speed)
};

//! Set Mode Command
static const CLI_Command_Definition_t xSetControlModeCommand =
{
    (const int8_t*)"mode",														//!< Text to type in command-line to exectue command
    (const int8_t*)"mode : Determines the control mode\r\nParameters: htho, ht, et, es, sm\r\n",					//!< Text to return on help
    &prvSetControlModeCommand,															//!< Function to run when command is typed
    1																			//!< Expected number of parameters (speed)
};

//! Sync Motor Command
static const CLI_Command_Definition_t xSyncMotorCommand =
{
    (const int8_t*)"sync",																			//!< Text to type in command-line to exectue command
    (const int8_t*)"sync : Orientates the motor to a known position in the electrical cycle.\r\n",	//!< Text to return on help
    &prvSyncMotorCommand,																				//!< Function to run when command is typed
    0																									//!< Expected number of parameters (speed)
};

//! Set commutation angle command
static const CLI_Command_Definition_t xSetCommutaionAngleCommand =
{
    (const int8_t*)"sca",																		//!< Text to type in command-line to exectue command
    (const int8_t*)"sca : Sets the commutation angle.\r\n",										//!< Text to return on help
    &prvSetCommutationAngleCommand,																//!< Function to run when command is typed
    1																							//!< Expected number of parameters (speed)
};

//! Enable velocity control command
static const CLI_Command_Definition_t xEnableVelocityControlCommand =
{
    (const int8_t*)"evc",																		//!< Text to type in command-line to exectue command
    (const int8_t*)"evc : Enables velocity control.\r\n",										//!< Text to return on help
    &prvEnableVelocityControlCommand,															//!< Function to run when command is typed
    0																							//!< Expected number of parameters (speed)
};

//! Set velocity command
static const CLI_Command_Definition_t xSetVelocityCommand =
{
    (const int8_t*)"sv",																		//!< Text to type in command-line to exectue command
    (const int8_t*)"sv : Changes the velocity set-point.\r\n",											//!< Text to return on help
    &prvSetVelocityCommand,																		//!< Function to run when command is typed
    1																							//!< Expected number of parameters (speed)
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
	FreeRTOS_CLIRegisterCommand(&xBldcOnCommand);
	FreeRTOS_CLIRegisterCommand(&xBldcOffCommand);
	FreeRTOS_CLIRegisterCommand(&xSetDutyCommand);
	FreeRTOS_CLIRegisterCommand(&xSetDirectionCommand);
	FreeRTOS_CLIRegisterCommand(&xSetControlModeCommand);
	FreeRTOS_CLIRegisterCommand(&xSyncMotorCommand);
	FreeRTOS_CLIRegisterCommand(&xSetCommutaionAngleCommand);
	FreeRTOS_CLIRegisterCommand(&xEnableVelocityControlCommand);
	FreeRTOS_CLIRegisterCommand(&xSetVelocityCommand);
	
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

//! @brief		Implements the bahaviour of the 'on' command
//! @details	Sends the BLDC_ON command word to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvBldcOnCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	//! @debug
	//PinCpDebugLed1_Write(0);

	portBASE_TYPE xResult;

	// Create command variable and input the command word 'BLDC_ON'
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_ON;

    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Motor turned on.\r\n\r\n" );
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_ON to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the 'off' CLI command
//! @details	Sends the BLDC_OFF command word to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvBldcOffCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{ 
	portBASE_TYPE xResult;
	
	// Create command variable and input the command word 'BLDC_ON'
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_OFF;

    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Motor turned off.\r\n\r\n" );
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_OFF to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the 'sduty' command
//! @details	Sends the BLDC_SET_DUTY command word and speed value to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvSetDutyCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
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
	
	
	// Create command variable and input the command word 'BLDC_ON'
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_SET_DUTY;
	
	float dutyP = atof((char*)pcParameter1);
	
	// Bounds checking
	if(dutyP < 0.0)
	{
		UartComms_PutString("ERROR: Make sure duty cycle is above 0%.\r\n\r\n");
		return pdFALSE;
	}
	else if(dutyP > 100.0)
	{
		UartComms_PutString("ERROR: Make sure duty cycle is below 100%.\r\n\r\n");
		return pdFALSE;
	}
	else
		bldcCommandStruct.value1 = dutyP;

    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Speed command received. Speed = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_SET_SPEED to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the set direction ('sdir') command
//! @details	Sends the BLDC_SET_SPEED command word and speed value to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvSetDirectionCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
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
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_SET_DIRECTION;
	
	if(!strcmp((char*)pcParameter1, "cw"))
	{
		bldcCommandStruct.value1 = (float)CLOCKWISE;
	}
	else if(!strcmp((char*)pcParameter1, "acw"))
	{
		bldcCommandStruct.value1 = (float)ANTI_CLOCKWISE;
	}
	else
	{
		UartComms_PutString("ERROR: Parameter to 'sd' not valid.\r\n\r\n");
		return pdFALSE;
	}
	
    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Direction command received. Direction = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_SET_DIRECTION to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the 'set-speed' command
//! @details	Sends the BLDC_SET_SPEED command word and speed value to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvSetControlModeCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
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
	
	#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
		UartDebug_PutString("COMMS: Set mode command received.\r\n");
	#endif
	
	// Create command variable and fill appropriate data
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_SET_CONTROL_MODE;
	
	if(!strcmp((char*)pcParameter1, "ht"))
	{
		bldcCommandStruct.value1 = (float)HALL_EFFECT_TRAPEZOIDAL;
	}
	else if(!strcmp((char*)pcParameter1, "et"))
	{
		bldcCommandStruct.value1 = (float)ENCODER_TRAPEZOIDAL;
	}
	else if(!strcmp((char*)pcParameter1, "es"))
	{
		bldcCommandStruct.value1 = (float)ENCODER_SINUSOIDAL;
	}
	else if(!strcmp((char*)pcParameter1, "sm"))
	{
		bldcCommandStruct.value1 = (float)STEP_MODE;
	}
	else
	{
		UartComms_PutString("ERROR: Parameter to 'mode' not valid.\r\n\r\n");
		return pdFALSE;
	}

    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Direction command received. Direction = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_ to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the 'sync motor' (sync) command
//! @details	
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvSyncMotorCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	portBASE_TYPE xResult;

	#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
		UartDebug_PutString("COMMS: Sync motor command received.\r\n");
	#endif
	
	// Create command variable and input the command word 'BLDC_ON'
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_SYNC_MOTOR;

    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Direction command received. Direction = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_ to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the set commutation angle ('sca') command
//! @details	Sends the BLDC_SET_COMMUTATION command word and angle value to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvSetCommutationAngleCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
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
	
	#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
		UartDebug_PutString("COMMS: Set commutation angle command received.\r\n");
	#endif

	float angle = atof((char*)pcParameter1);
	
	// Bounds checking
	if(angle == 0.0)
	{
		UartComms_PutString("ERROR: Parameter to 'sca' not valid.\r\n\r\n");
	}
	else if(angle < 0.0)
	{
		UartComms_PutString("ERROR: Please make sure commutation angle is positive. Use 'set direction (sd)' command to change direction of rotation.\r\n\r\n");
	}
	else if(angle >= 360.0)
	{
		UartComms_PutString("ERROR: Please make sure commutation angle is less than 360.\r\n\r\n");
	}

	// Create command variable and fill appropriate data
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_SET_COMMUTATION_ANGLE;
	bldcCommandStruct.value1 = angle;
	
    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Direction command received. Direction = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_ to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the 'enable velocity control' (evc) command
//! @details	
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvEnableVelocityControlCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
	//const int8_t *pcParameter1;
	//portBASE_TYPE xParameter1StringLength; 
	portBASE_TYPE xResult;

	#if(configPRINT_DEBUG_COMMS_INTERFACE == 1)
		UartDebug_PutString("COMMS: Enable velocity control command received.\r\n");
	#endif
	
	// Create command variable and input the command word 'BLDC_ON'
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_ENABLE_VELOCITY_CONTROL;

    // Send command to the BLDC task to turn motor on
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Direction command received. Direction = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Error sending command BLDC_ENABLE_VELOCITY_CONTROL to motor\r\n\r\n" );
    }

    // There is only a single line of output produced in all cases. pdFALSE is
    // returned because there is no more output to be generated.
    return pdFALSE;
}

//! @brief		Implements the bahaviour of the set velocity ('sv') command
//! @details	Sends the BLDC_SET_VELOCITY command word and velocity value to the BLDC task through the queue
//! @note 		Must have the correct prototype. Public by way of function pointer.
//! @public
static portBASE_TYPE prvSetVelocityCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
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
	
	uint16 velocitySetPointRpm = atof((char*)pcParameter1);
	
	// Bounds checking
	if(velocitySetPointRpm < 0.0)
	{
		UartComms_PutString("ERROR: Velocity set point has to be > 0.\r\n\r\n");
		return pdFALSE;	
	}
	else if(velocitySetPointRpm > configBLDC_MAX_RPM)
	{
		snprintf((char*)pcWriteBuffer, xWriteBufferLen, "ERROR: Velocity set point has to be lower than %f\r\n\r\n", configBLDC_MAX_RPM);
		return pdFALSE;
	}
	
	// Create command variable and input the command word 'BLDC_SET_VELOCITY'
	bldcCommandStruct_t bldcCommandStruct;
	bldcCommandStruct.commandWord = BLDC_SET_VELOCITY;
	bldcCommandStruct.value1 = velocitySetPointRpm;
	
    // Send command to the BLDC task to change it's velocity
    xResult = xQueueSendToBack(_bldcTaskCommandQueue, &bldcCommandStruct , configMAX_QUEUE_WAIT_TIME_MS_BLDC_TASK/portTICK_RATE_MS);

    if(xResult == pdPASS)
    {
        // The command was successful.  There is nothing to output.
        // The copy was not successful. Inform the user.
        //snprintf((char*)pcWriteBuffer, xWriteBufferLen, "Speed command received. Speed = %u\r\n\r\n", atoi((char*)pcParameter1));
    }
    else
    {
        // The copy was not successful. Inform the user.
        snprintf((char*)pcWriteBuffer, xWriteBufferLen, "ERROR: Could not send command BLDC_SET_VELOCITY to motor\r\n\r\n" );
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