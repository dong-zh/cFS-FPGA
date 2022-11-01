// Code for handling the FPGA interrupt child task.

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#include "cfe.h"
#include "mmio_lib.h"
#include "fpga_ctrl.h"

// Defined in fpga_ctrl.c
extern FPGA_CTRL_Data_t globalState;

// Exported function
int32 FPGA_CTRL_IntCtrl(FPGA_CTRL_IntCtrlCmd_t const *SBBufPtr);

static void  FPGA_CTRL_WaitForButton(void);
static int32 FPGA_CTRL_ResetInterrupts(int uio, uint32 volatile *isr);
static void  FPGA_CTRL_FullCleanupChildTaskAndExit(int uioFd, void volatile *btnBase, cpusize btnBaseMapRange,
                                                   void volatile *swBase, cpusize swBaseMapRange);
static void  FPGA_CTRL_ExitChildTask(void);

int32 FPGA_CTRL_IntCtrl(FPGA_CTRL_IntCtrlCmd_t const *SBBufPtr)
{
    int32      err;
    bool const doEnable = SBBufPtr->enable;

    if (doEnable)
    {
        if (globalState.childTaskRunning)
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: Interrupts already enabled");
            return CFE_ES_ERR_CHILD_TASK_CREATE;
        }

        CFE_ES_TaskId_t childTaskId;
        if ((err = CFE_ES_CreateChildTask(&childTaskId, "FPGA_CTRL child", FPGA_CTRL_WaitForButton,
                                          CFE_ES_TASK_STACK_ALLOCATE, CFE_PLATFORM_ES_DEFAULT_STACK_SIZE,
                                          CFE_PLATFORM_ES_PERF_CHILD_PRIORITY, 0)) < CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: Failed to create child task: 0x%x", err);
            globalState.childTaskId = CFE_ES_TASKID_UNDEFINED;
            return err;
        }

        globalState.childTaskId         = childTaskId;
        globalState.childTaskShouldExit = false;
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "FPGA_CTRL: Spawned child task, child task ID: 0x%x", childTaskId);
    }
    else
    {
        if (!globalState.childTaskRunning)
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL_IntCtrl: Interrupts already disabled");
            return CFE_ES_ERR_CHILD_TASK_DELETE;
        }

        // Tell the child to exit next time it wakes
        globalState.childTaskShouldExit = true;
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "FPGA_CTRL: Marked child task %d to exit", globalState.childTaskId);
    }

    return CFE_SUCCESS;
}

// Enable interrupts and infinitely loop waiting for interrupts.
static void FPGA_CTRL_WaitForButton(void)
{
    int32 err;
    globalState.childTaskRunning = true;
    // #define FPGA_INTERRUPTS_TEST
#ifndef FPGA_INTERRUPTS_TEST

    // Addresses and sizes
    static cpuaddr const AXI_BTN_BASE = 0x41200000; // Base address of the AXI GPIO for the buttons
    static cpuaddr const AXI_SW_BASE  = 0x41210000; // Base address of the AXI GPIO for the switches
    static cpusize const MAP_RANGE    = 0x10000;

    static cpusize const GIER_OFFSET = 0x11c; // Global interrupt enable register
    static cpusize const IER_OFFSET  = 0x128; // Interrupt enable register
    static cpusize const ISR_OFFSET  = 0x120; // Interrupt status register

    // Masks
    static uint32 const GIER_ENABLE_MASK    = 0x80000000; // Enable interrupts
    static uint32 const IER_CH1_ENABLE_MASK = 0x1;        // Enable interrupts on channel 1
    // static uint32 const ISR_CH1_MASK        = 0x1;        // Mask for channel 1

    void *const btnBase = NULL; // Base address of the AXI GPIO for the buttons
    void *const swBase  = NULL; // Base address of the AXI GPIO for the switches
    // TODO - Platform specific function, move to library?
    OS_printf("FPGA_CTRL: opening uio0...\n");
    int uioFd = open("/dev/uio0", O_RDWR | O_SYNC);
    if (uioFd < 0)
    {
        // Error, cleanup and exit
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "Failed to open UIO device, uioFD = %d, exiting child...", uioFd);
        FPGA_CTRL_ExitChildTask();
    }

    OS_printf("FPGA_CTRL: mapping btnBase...\n");
    if ((err = mmio_lib_NewMapping((void **)&btnBase, AXI_BTN_BASE, MAP_RANGE)) < CFE_SUCCESS)
    {
        // Error, cleanup and exit
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "Failed to map button GPIO, err = %d, exiting child...", err);
        if (close(uioFd) < 0)
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "Failed to close UIO device, uioFD = %d, exiting child...", uioFd);
        FPGA_CTRL_ExitChildTask();
    }

    OS_printf("FPGA_CTRL: mapping swBase...\n");
    if ((err = mmio_lib_NewMapping((void **)&swBase, AXI_SW_BASE, MAP_RANGE)) < CFE_SUCCESS)
    {
        // Error, cleanup and exit
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "Failed to map switch GPIO, err = %d, exiting child...", err);
        if (close(uioFd) < 0)
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "Failed to close UIO device, uioFD = %d, exiting child...", uioFd);
        if (mmio_lib_DeleteMapping((void *)btnBase, MAP_RANGE) < CFE_SUCCESS)
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "Failed to unmap button GPIO, exiting child...");
        FPGA_CTRL_ExitChildTask();
    }

    uint32 volatile *const gier =
        (uint32 volatile *const)((cpuaddr)btnBase + (cpusize)GIER_OFFSET); // Global interrupt enable register
    uint32 volatile *const ier =
        (uint32 volatile *const)((cpuaddr)btnBase + (cpusize)IER_OFFSET); // Interrupt enable register
    uint32 volatile *const isr =
        (uint32 volatile *const)((cpuaddr)btnBase + (cpusize)ISR_OFFSET); // Interrupt status register

    OS_printf("FPGA_CTRL: enabling interrupts by setting registers...\n");
    // FIXME - the following line segfaults with compiled with -O
    *gier |= GIER_ENABLE_MASK;   // Enable global interrupts
    *ier |= IER_CH1_ENABLE_MASK; // Enable interrupts on channel 1

    if (FPGA_CTRL_ResetInterrupts(uioFd, isr) < CFE_SUCCESS)
        FPGA_CTRL_FullCleanupChildTaskAndExit(uioFd, btnBase, MAP_RANGE, swBase, MAP_RANGE);

    OS_printf("FPGA_CTRL: Clearing UIO interrupt...\n");
    uint32 const one = 1;
    if (write(uioFd, &one, sizeof(one)) != sizeof(one))
    {
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "Failed to clear UIO interrupt, exiting child...");
        FPGA_CTRL_FullCleanupChildTaskAndExit(uioFd, btnBase, MAP_RANGE, swBase, MAP_RANGE);
    }
    // Only send message on button down to avoid duplicate messages since button up generates an interrupt as well
    bool lastButtonPressed = !!(*(uint8 volatile *)btnBase); // Read the button position
#endif

    // Wait on interrupt loop
    do
    {
        OS_printf("FPGA_CTRL: Waiting for interrupt...\n");
#ifdef FPGA_INTERRUPTS_TEST
        OS_TaskDelay(2000); // Pretend to trigger an interrupt every 2 seconds

        // Dummy variables for testing on a normal computer
        uint8 const  switchPos         = 0xab;       // Dummy switch position
        uint32 const buf               = 0xffffffff; // Dummy buffer of all 1s
        bool const   lastButtonPressed = false;      // Pretend the button wasn't pressed
        bool const   buttonPressed     = true;       // Pretend the button is pressed
#else

        // Actual interrupt checking code
        struct pollfd pollFd = {
            .fd     = uioFd,
            .events = POLLIN,
        };
        // Block on poll() until interrupt or timeout of 5 seconds
        // Has to timeout to check whether the child task should exit
        if ((err = poll(&pollFd, 1, 5000)) < 0)
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: Error polling for interrupt: %d", err);
            break;
        }

        if (FPGA_CTRL_ResetInterrupts(uioFd, isr) < CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: Error resetting interrupts");
            break;
        }

#endif
        if (globalState.childTaskShouldExit)
        {
            OS_printf("FPGA_CTRL: Child should quit, breaking loop\n");
            break;
        }

        // poll() returned, now check whether it was a timeout, an interrupt, or some error

#ifndef FPGA_INTERRUPTS_TEST
        if (err == 0)
        {
            // Do nothing and continue to loop if timeout
            OS_printf("FPGA_CTRL: Timed out waiting for interrupt\n");
            continue;
        }
        if (!(pollFd.revents & POLLIN))
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: Error polling for interrupt: %d", pollFd.revents);
            break;
        }

        // If execution reaches here, then an interrupt occurred
        // Now handle the interrupt

        uint32 buf;
        // read() shouldn't block since poll() returned
        if (read(uioFd, &buf, sizeof(buf)) < 0)
        {
            CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: Error reading interrupt: %d", err);
            break;
        }

        uint8 const switchPos = *(uint8 const *)swBase; // Read the switch position

        bool const buttonPressed = !!(*(uint8 volatile *)btnBase); // Read the button position
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "FPGA_CTRL: Button interrupt! Switch position: 0x%02x, last button position %d, current "
                          "button position %d, read 0x%x from the uioFd",
                          switchPos, lastButtonPressed, buttonPressed, buf);
#endif

        // Send telemetry packet on rising edge (button pressed down)
        if (!lastButtonPressed && buttonPressed)
        {
            OS_printf("FPGA_CTRL: Sending telemetry packet...\n");
            FPGA_CTRL_IntTlm_t telemetryPacket;
            if ((err = CFE_MSG_Init(&telemetryPacket.TlmHeader.Msg, CFE_SB_ValueToMsgId(FPGA_CTRL_INT_TLM_MID),
                                    sizeof(telemetryPacket)) < CFE_SUCCESS))
            {
                CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                                  "FPGA_CTRL: Error initializing telemetry packet: %d", err);
            }
            telemetryPacket.switchPos = switchPos;
            CFE_SB_TimeStampMsg((CFE_MSG_Message_t *)&telemetryPacket);
            if ((err = CFE_SB_TransmitMsg((CFE_MSG_Message_t *)&telemetryPacket, true)) < CFE_SUCCESS)
                CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                                  "FPGA_CTRL: Failed to send telemetry packet, error: 0x%08x", err);
        }

#ifndef FPGA_INTERRUPTS_TEST
        lastButtonPressed = buttonPressed;
#endif
    } while (true);

#ifndef FPGA_INTERRUPTS_TEST
    FPGA_CTRL_FullCleanupChildTaskAndExit(uioFd, btnBase, MAP_RANGE, swBase, MAP_RANGE);
#endif
}

static void FPGA_CTRL_ExitChildTask(void)
{
    globalState.childTaskRunning = false;
    // globalState.childTaskId      = CFE_ES_TASKID_UNDEFINED;
    CFE_ES_ExitChildTask();                // This shouldn't return
    CFE_PSP_Panic(CFE_ES_NOT_IMPLEMENTED); // Panic if it does return
}

static void FPGA_CTRL_FullCleanupChildTaskAndExit(int uioFd, void volatile *btnBase, cpusize btnBaseMapRange,
                                                  void volatile *swBase, cpusize swBaseMapRange)
{
    CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "FPGA_CTRL: Fully cleaning up and exiting child..");
    if (close(uioFd) < 0)
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "FPGA_CTRL: Failed to close UIO device, uioFD = %d", uioFd);
    if (mmio_lib_DeleteMapping((void *)btnBase, btnBaseMapRange) < CFE_SUCCESS)
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR, "FPGA_CTRL: Failed to unmap button GPIO");
    if (mmio_lib_DeleteMapping((void *)swBase, swBaseMapRange) < CFE_SUCCESS)
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR, "FPGA_CTRL: Failed to unmap switch GPIO");
    FPGA_CTRL_ExitChildTask(); // This shouldn't return
}

static int32 FPGA_CTRL_ResetInterrupts(int const uio, uint32 volatile *const isr)
{
    OS_printf("FPGA_CTRL: Resetting interrupts...\n");
    static uint32 const ISR_CH1_MASK = 0x1; // Mask for channel 1

    if (*isr & ISR_CH1_MASK)
    {
        OS_printf("FPGA_CTRL: Clearing interrupt on channel 1...\n");
        *isr |= ISR_CH1_MASK;
    }
    else
    {
        OS_printf("FPGA_CTRL: No interrupt pending on channel 1.\n");
    }

    return CFE_SUCCESS;
}
