#include <cfe.h>

// Loads a bitstream using a script in the home directory.
// Completely breaks cFS's abstractions, but it's just for the demo.
int32 FPGA_CTRL_LoadBitstream(FPGA_CTRL_ReprogramCmd_t const *SBBufPtr)
{
    static char const *const SCRIPT_PATH   = "/home/ubuntu/program-fpga.sh";
    char const *const        bitstreamPath = SBBufPtr->path;

    BUGCHECK(bitstreamPath != NULL, OS_INVALID_POINTER);

    int32 err;

    char buf[256];
    if ((err = snprintf(buf, sizeof(buf), "bash -c \"%s %s\"", SCRIPT_PATH, bitstreamPath)) >= sizeof(buf))
    {
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "Command to program FPGA is too long (%d bytes)", err);
        return CFE_FS_FNAME_TOO_LONG;
    }
    CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION, "Executing command %s", buf);

    // This is very bad
    if ((err = system(buf)))
    {
        CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_ERROR,
                          "Failed to load bitstream file %s, err = %d", bitstreamPath, err);
        return err;
    }

    CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION, "Loaded bitstream file %s",
                      bitstreamPath);

    return CFE_SUCCESS;
}
