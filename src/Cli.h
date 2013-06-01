//!
//! @file 		Cli.h
//! @author 	Geoffrey Hunter <gbmhunter@gmail.com> (www.cladlab.com)
//! @date 		2012/09/12
//! @brief 		Implements the behaviour of comms commands, sends commands via queues to other tasks.
//!				Talks to the FreeRTOS CLI (FreeRTOS_CLI.c)
//! @details
//!		<b>Last Modified:			</b> 2012/10/09					\n
//!		<b>Version:					</b> v1.0.0.0					\n
//!		<b>Company:					</b> CladLabs					\n
//!		<b>Project:					</b> Free Code Libraries		\n
//!		<b>Language:				</b> C							\n
//!		<b>Compiler:				</b> GCC						\n
//! 	<b>uC Model:				</b> PSoC5						\n
//!		<b>Computer Architecture:	</b> ARM						\n
//! 	<b>Operating System:		</b> FreeRTOS					\n
//!		<b>License:					</b> GPLv3						\n
//!		<b>Documentation Format:	</b> Doxygen					\n
//!

void Cli_Start(uint32 mainCliTaskStackSize, uint8 mainCliTaskPriority);

// EOF
