#include "NUBLX_SYS_1002.h"
#include "McuRTOS.h"
#include "FreeRTOS_IP.h"

#include "logging.h"
#include "demo_config.h"
#include "transport_interface.h"

#include "artya7.h"

const uint8_t IPADDRESS[4]   = {192, 168, 0, 17};
const uint8_t MASK[4]         = {255, 255, 255, 0};
const uint8_t GATEWAY[4]      = {192, 168, 0, 1};
const uint8_t DNS[4]          = {192, 168, 0, 1};

const uint8_t MAC[6]          = {0x00, 0x00, 0x5E, 0x00, 0xFA, 0xCE};



extern void vStartSimpleHTTPDemo(void);

extern void vPlatformInitIpStack(void);


int main(void)
{
    BaseType_t state;

    vPlatformInitLogging(); // Initialize logging


    // Initialize the FreeRTOS+TCP Stack
    vPlatformInitIpStack();


    state = xTaskGetSchedulerState();

    if(state == taskSCHEDULER_NOT_STARTED)
    {
        vTaskStartScheduler();
    }

    while(1);

    return 0;
}

void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent)
{
    if(eNetworkEvent == eNetworkUp)
    {
        ARTYA7_LEDx(2, 1);

        vStartSimpleHTTPDemo();

        vTaskStartScheduler();
    }
    else if(eNetworkEvent == eNetworkDown)
    {

    }
}

void vPlatformInitIpStack()
{
    BaseType_t ipState;

    ipState = FreeRTOS_IPInit(IPADDRESS, MASK, GATEWAY, DNS, MAC);

    // Check if IP stack initialization was successful
    if(ipState != pdPASS)
    {
        // Handle initialization failure
        // For example, log an error message or take appropriate action
    }
}


void vPlatformInitLogging( void )
{
	vLoggingInit( pdTRUE, pdFALSE, pdFALSE, 0U, 0U );

	BaseType_t xStdoutLoggingUsed = pdFALSE, xDiskFileLoggingUsed = pdFALSE, xUDPLoggingUsed = pdFALSE;

}


