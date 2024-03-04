#include "logging.h"
#include "FreeRTOS_IP.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>


#define dlMAX_PRINT_STRING_LENGTH 50
#define socklen_t uint32_t
#define  xLogStreamBuffer 50

void xPrintSocket()
{
	int num;

	num = 0;
}



void vLoggingInit( BaseType_t xLogToStdout,
                   BaseType_t xLogToFile,
                   BaseType_t xLogToUDP,
                   uint32_t ulRemoteIPAddress,
                   uint16_t usRemotePort )
{
    /* Can only be called before the scheduler has started. */
    configASSERT( xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED );

    #if ( ( ipconfigHAS_DEBUG_PRINTF == 1 ) || ( ipconfigHAS_PRINTF == 1 ) )
    {
        HANDLE Win32Thread;

        /* Record which output methods are to be used. */
        xStdoutLoggingUsed = xLogToStdout;
        xDiskFileLoggingUsed = xLogToFile;
        xUDPLoggingUsed = xLogToUDP;

        /* If a disk file is used then initialize it now. */
        if( xDiskFileLoggingUsed != pdFALSE )
        {
            prvFileLoggingInit();
        }

        /* If UDP logging is used then store the address to which the log data
         * will be sent - but don't create the socket yet because the network is
         * not initialized. */
        if( xUDPLoggingUsed != pdFALSE )
        {
            /* Set the address to which the print messages are sent. */
            xPrintUDPAddress.sin_port = FreeRTOS_htons( usRemotePort );

            #if defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
            {
                xPrintUDPAddress.sin_address.ulIP_IPv4 = ulRemoteIPAddress;
            }
            #else
            {
                xPrintUDPAddress.sin_addr = ulRemoteIPAddress;
            }
            #endif /* defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */

            xPrintUDPAddress.sin_family = FREERTOS_AF_INET;
        }

        /* If a disk file or stdout are to be used then Win32 system calls will
         * have to be made.  Such system calls cannot be made from FreeRTOS tasks
         * so create a stream buffer to pass the messages to a Win32 thread, then
         * create the thread itself, along with a Win32 event that can be used to
         * unblock the thread. */
        if( ( xStdoutLoggingUsed != pdFALSE ) || ( xDiskFileLoggingUsed != pdFALSE ) )
        {
            /* Create the buffer. */
            xLogStreamBuffer = ( StreamBuffer_t * ) malloc( sizeof( *xLogStreamBuffer ) - sizeof( xLogStreamBuffer->ucArray ) + dlLOGGING_STREAM_BUFFER_SIZE + 1 );
            configASSERT( xLogStreamBuffer );
            memset( xLogStreamBuffer, '\0', sizeof( *xLogStreamBuffer ) - sizeof( xLogStreamBuffer->ucArray ) );
            xLogStreamBuffer->LENGTH = dlLOGGING_STREAM_BUFFER_SIZE + 1;

            /* Create the Windows event. */
            pvLoggingThreadEvent = CreateEvent( NULL, FALSE, TRUE, "StdoutLoggingEvent" );

            /* Create the thread itself. */
            Win32Thread = CreateThread(
                NULL,                  /* Pointer to thread security attributes. */
                0,                     /* Initial thread stack size, in bytes. */
                prvWin32LoggingThread, /* Pointer to thread function. */
                NULL,                  /* Argument for new thread. */
                0,                     /* Creation flags. */
                NULL );

            /* Use the cores that are not used by the FreeRTOS tasks. */
            SetThreadAffinityMask( Win32Thread, ~0x01u );
            SetThreadPriorityBoost( Win32Thread, TRUE );
            SetThreadPriority( Win32Thread, THREAD_PRIORITY_IDLE );
        }
    }
    #else /* if ( ( ipconfigHAS_DEBUG_PRINTF == 1 ) || ( ipconfigHAS_PRINTF == 1 ) ) */
    {
        /* FreeRTOSIPConfig is set such that no print messages will be output.
         * Avoid compiler warnings about unused parameters. */
        ( void ) xLogToStdout;
        ( void ) xLogToFile;
        ( void ) xLogToUDP;
        ( void ) usRemotePort;
        ( void ) ulRemoteIPAddress;
    }
    #endif /* ( ipconfigHAS_DEBUG_PRINTF == 1 ) || ( ipconfigHAS_PRINTF == 1 )  */
}

void vLoggingPrintf( const char * pcFormat,
                     ... )
{
    char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];
    char cOutputString[ dlMAX_PRINT_STRING_LENGTH ];
    char * pcSource, * pcTarget, * pcBegin;
    size_t xLength, xLength2, rc;
    static BaseType_t xMessageNumber = 0;
    static BaseType_t xAfterLineBreak = pdTRUE;
    va_list args;
    uint32_t ulIPAddress;
    const char * pcTaskName;
    const char * pcNoTask = "None";
    int iOriginalPriority;
//    HANDLE xCurrentTask;


    if( ( xStdoutLoggingUsed != pdFALSE ) || ( xDiskFileLoggingUsed != pdFALSE ) || ( xUDPLoggingUsed != pdFALSE ) )
    {
        /* There are a variable number of parameters. */
        va_start( args, pcFormat );

        /* Additional info to place at the start of the log. */
        if( xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED )
        {
            pcTaskName = pcTaskGetName( NULL );
        }
        else
        {
            pcTaskName = pcNoTask;
        }

        if( ( xAfterLineBreak == pdTRUE ) && ( strcmp( pcFormat, "\r\n" ) != 0 ) )
        {
            xLength = snprintf( cPrintString, dlMAX_PRINT_STRING_LENGTH, "%lu %lu [%s] ",
                                xMessageNumber++,
                                ( unsigned long ) xTaskGetTickCount(),
                                pcTaskName );
            xAfterLineBreak = pdFALSE;
        }
        else
        {
            xLength = 0;
            memset( cPrintString, 0x00, dlMAX_PRINT_STRING_LENGTH );
            xAfterLineBreak = pdTRUE;
        }

        xLength2 = vsnprintf( cPrintString + xLength, dlMAX_PRINT_STRING_LENGTH - xLength, pcFormat, args );

        if( xLength2 < 0 )
        {
            /* Clean up. */
            xLength2 = dlMAX_PRINT_STRING_LENGTH - 1 - xLength;
            cPrintString[ dlMAX_PRINT_STRING_LENGTH - 1 ] = '\0';
        }

        xLength += xLength2;
        va_end( args );

        /* For ease of viewing, copy the string into another buffer, converting
         * IP addresses to dot notation on the way. */
        pcSource = cPrintString;
        pcTarget = cOutputString;

        while( ( *pcSource ) != '\0' )
        {
            *pcTarget = *pcSource;
            pcTarget++;
            pcSource++;

            /* Look forward for an IP address denoted by 'ip'. */
            if( ( isxdigit( pcSource[ 0 ] ) != pdFALSE ) && ( pcSource[ 1 ] == 'i' ) && ( pcSource[ 2 ] == 'p' ) )
            {
                *pcTarget = *pcSource;
                pcTarget++;
                *pcTarget = '\0';
                pcBegin = pcTarget - 8;

                while( ( pcTarget > pcBegin ) && ( isxdigit( pcTarget[ -1 ] ) != pdFALSE ) )
                {
                    pcTarget--;
                }

                sscanf( pcTarget, "%8X", &ulIPAddress );
                rc = sprintf( pcTarget, "%lu.%lu.%lu.%lu",
                              ( unsigned long ) ( ulIPAddress >> 24UL ),
                              ( unsigned long ) ( ( ulIPAddress >> 16UL ) & 0xffUL ),
                              ( unsigned long ) ( ( ulIPAddress >> 8UL ) & 0xffUL ),
                              ( unsigned long ) ( ulIPAddress & 0xffUL ) );
                pcTarget += rc;
                pcSource += 3; /* skip "<n>ip" */
            }
        }
        TaskFunction_t xFunctionToPend;


        TickType_t xTicksToWait;
        const struct freertos_sockaddr *pxDestinationAddress;
        socklen_t xDestinationAddressLength;

        /* How far through the buffer was written? */

        xLength = ( BaseType_t ) ( pcTarget - cOutputString );

        /* If the message is to be logged to a UDP port then it can be sent directly
         * because it only uses FreeRTOS function (not Win32 functions). */
        if( xUDPLoggingUsed != pdFALSE )
        {
            if( ( xPrintSocket() == FREERTOS_INVALID_SOCKET() ) && ( FreeRTOS_IsNetworkUp() != pdFALSE ) )
            {
                /* Create and bind the socket to which print messages are sent.  The
                 * xTimerPendFunctionCall() function is used even though this is
                 * not an interrupt because this function is called from the IP task
                 * and the	IP task cannot itself wait for a socket to bind.  The
                 * parameters to prvCreatePrintSocket() are not required so set to
                 * NULL or 0. */
                xTimerPendFunctionCall( xFunctionToPend, NULL, 0, xTicksToWait );
            }

            if( xPrintSocket != FREERTOS_INVALID_SOCKET() )
            {
                FreeRTOS_sendto( xPrintSocket, cOutputString, xLength, 0, &pxDestinationAddress, sizeof( xDestinationAddressLength ) );

                /* Just because the UDP data logger I'm using is dumb. */
                FreeRTOS_sendto( xPrintSocket, "\r", sizeof( char ), 0, &pxDestinationAddress, sizeof( xDestinationAddressLength ) );
            }
        }

        /* If logging is also to go to either stdout or a disk file then it cannot
         * be output here - so instead write the message to the stream buffer and wake
         * the Win32 thread which will read it from the stream buffer and perform the
         * actual output. */
        if( ( xStdoutLoggingUsed != pdFALSE ) || ( xDiskFileLoggingUsed != pdFALSE ) )
        {
            configASSERT( xLogStreamBuffer );

            /* How much space is in the buffer? */
            xLength2 = uxStreamBufferGetSpace( xLogStreamBuffer );

            /* There must be enough space to write both the string and the length of
             * the string. */
            if( xLength2 >= ( xLength + sizeof( xLength ) ) )
            {
                /* First write in the length of the data, then write in the data
                 * itself.  Raising the thread priority is used as a critical section
                 * as there are potentially multiple writers.  The stream buffer is
                 * only thread safe when there is a single writer (likewise for
                 * reading from the buffer). */
//                xCurrentTask = GetCurrentThread();
//                iOriginalPriority = GetThreadPriority( xCurrentTask );
//                SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
                uxStreamBufferAdd( xLogStreamBuffer, 0, ( const uint8_t * ) &( xLength ), sizeof( xLength ) );
                uxStreamBufferAdd( xLogStreamBuffer, 0, ( const uint8_t * ) cOutputString, xLength );
                SetThreadPriority( GetCurrentThread(), iOriginalPriority );
            }

            /* xDirectPrint is initialized to pdTRUE, and while it remains true the
             * logging output function is called directly.  When the system is running
             * the output function cannot be called directly because it would get
             * called from both FreeRTOS tasks and Win32 threads - so instead wake the
             * Win32 thread responsible for the actual output. */

            BaseType_t xDirectPrint;
            BaseType_t pvLoggingThreadEvent;

            if( xDirectPrint != pdFALSE )
            {
                /* While starting up, the thread which calls prvWin32LoggingThread()
                 * is not running yet and xDirectPrint will be pdTRUE. */
                prvLoggingFlushBuffer();
            }
            else if( pvLoggingThreadEvent != NULL )
            {
                /* While running, wake up prvWin32LoggingThread() to send the
                 * logging data. */
                SetEvent( pvLoggingThreadEvent );
            }
        }
    }
}
/*-----------------------------------------------------------*/
//
//static void prvLoggingFlushBuffer( void )
//{
//    size_t xLength;
//    char cPrintString[ dlMAX_PRINT_STRING_LENGTH ];
//
//    /* Is there more than the length value stored in the circular buffer
//     * used to pass data from the FreeRTOS simulator into this Win32 thread? */
//    while( uxStreamBufferGetSize( xLogStreamBuffer ) > sizeof( xLength ) )
//    {
//        memset( cPrintString, 0x00, dlMAX_PRINT_STRING_LENGTH );
//        uxStreamBufferGet( xLogStreamBuffer, 0, ( uint8_t * ) &xLength, sizeof( xLength ), pdFALSE );
//        uxStreamBufferGet( xLogStreamBuffer, 0, ( uint8_t * ) cPrintString, xLength, pdFALSE );
//
//        /* Write the message to standard out if requested to do so when
//         * vLoggingInit() was called, or if the network is not yet up. */
//        if( ( xStdoutLoggingUsed != pdFALSE ) || ( FreeRTOS_IsNetworkUp() == pdFALSE ) )
//        {
//            /* Write the message to stdout. */
//            _write( _fileno( stdout ), cPrintString, strlen( cPrintString ) );
//        }
//
//        /* Write the message to a file if requested to do so when
//         * vLoggingInit() was called. */
//        if( xDiskFileLoggingUsed != pdFALSE )
//        {
//            prvLogToFile( cPrintString, xLength );
//        }
//    }
//
//    prvFileClose();
//}
///*-----------------------------------------------------------*/
//
//static DWORD WINAPI prvWin32LoggingThread( void * pvParameter )
//{
//    const DWORD xMaxWait = 1000;
//
//    ( void ) pvParameter;
//
//    /* From now on, prvLoggingFlushBuffer() will only be called from this
//     * Windows thread */
//    xDirectPrint = pdFALSE;
//
//    for( ; ; )
//    {
//        /* Wait to be told there are message waiting to be logged. */
//        WaitForSingleObject( pvLoggingThreadEvent, xMaxWait );
//
//        /* Write out all waiting messages. */
//        prvLoggingFlushBuffer();
//    }
//}
///*-----------------------------------------------------------*/
//
//static void prvFileLoggingInit( void )
//{
//    FILE * pxHandle = fopen( pcLogFileName, "a" );
//
//    if( pxHandle != NULL )
//    {
//        fseek( pxHandle, SEEK_END, 0ul );
//        ulSizeOfLoggingFile = ftell( pxHandle );
//        fclose( pxHandle );
//    }
//    else
//    {
//        ulSizeOfLoggingFile = 0ul;
//    }
//}
