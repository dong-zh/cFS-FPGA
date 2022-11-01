#include <string.h>

#include "cfe.h"

#include "mmio_lib.h"

int32 FPGA_CTRL_Encrypt(FPGA_CTRL_EncryptCmd_t const *Msg);

static void FPGA_CTRL_BinToHexStr(char *hexStr, uint8 const *bin, cpusize binLen);

int32 FPGA_CTRL_Encrypt(FPGA_CTRL_EncryptCmd_t const *Msg)
{
#define AES_BLOCK_SIZE 0x10
    int32 err;

    // Control register masks
    static uint8 const AP_START = 0x01; // Start the encryption
    static uint8 const AP_DONE  = 0x02; // Encryption is done
    // static uint8 const AP_IDLE      = 0x04; // Encryption is idle
    // static uint8 const AP_READY     = 0x08; // Encryption is ready
    // static uint8 const AUTO_RESTART = 0x10; // Automatically restart the encryption after it is done

    // Addresses and sizes
    static cpuaddr const CONTROL_REGISTER_BASE  = 0x43c00000;
    static cpuaddr const IN_BASE                = 0x43c10000;
    static cpuaddr const OUT_BASE               = 0x43c20000;
    static cpusize const MAP_RANGE              = 0x10000;
    static cpusize const KEY_BASE_OFFSET        = 0x20;
    static cpusize const PLAINTEXT_BASE_OFFSET  = 0x10;
    static cpusize const CYPHERTEXT_BASE_OFFSET = 0x10;

    static uint8 const KEY[AES_BLOCK_SIZE] = {
        0x2b, 0x28, 0xab, 0x09, 0x7e, 0xae, 0xf7, 0xcf, 0x15, 0xd2, 0x15, 0x4f, 0x16, 0xa6, 0x88, 0x3c,
    };

    char const *const plaintext = Msg->data;

    uint8 volatile *const controlReg = NULL;
    void volatile *const  inBlk      = NULL;
    void volatile *const  outBlk     = NULL;
    if ((err = mmio_lib_NewMapping((void **)&controlReg, CONTROL_REGISTER_BASE, MAP_RANGE)) < CFE_SUCCESS)
        return err;
    if ((err = mmio_lib_NewMapping((void **)&inBlk, IN_BASE, MAP_RANGE)) < CFE_SUCCESS)
        return err;
    if ((err = mmio_lib_NewMapping((void **)&outBlk, OUT_BASE, MAP_RANGE)) < CFE_SUCCESS)
        return err;

    void volatile *const       plaintextReg = (void volatile *const)((cpuaddr)inBlk + (cpusize)PLAINTEXT_BASE_OFFSET);
    void volatile *const       keyReg       = (void volatile *const)((cpuaddr)inBlk + (cpusize)KEY_BASE_OFFSET);
    void const volatile *const cyphertextReg =
        (void volatile *const)((cpuaddr)outBlk + (cpusize)CYPHERTEXT_BASE_OFFSET);

    OS_printf("Copying key to PL...\n");
    memcpy((void *)keyReg, (void *)KEY, AES_BLOCK_SIZE);

    OS_printf("Copying plaintext to PL...\n");
    memcpy((void *)plaintextReg, (void *)plaintext, AES_BLOCK_SIZE);

    OS_printf("Control register = 0x%02x\n", *controlReg);

    char plaintextHex[AES_BLOCK_SIZE * 2 + 1]; // 2 hex digits per byte + null terminator
    FPGA_CTRL_BinToHexStr(plaintextHex, (uint8 const *)plaintext, AES_BLOCK_SIZE);

    CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION, "Starting encryption on %s...",
                      plaintextHex);
    CFE_TIME_SysTime_t const startTime = CFE_TIME_GetTime();
    *controlReg |= AP_START;

    // Poll for AP_DONE
    while (!(*controlReg & AP_DONE))
    {
        // Busy wait
    }

    CFE_TIME_SysTime_t const timeTaken      = CFE_TIME_Subtract(CFE_TIME_GetTime(), startTime);
    uint32 const             microSecsTaken = CFE_TIME_Sub2MicroSecs(timeTaken.Subseconds);

    OS_printf("Control register = 0x%02x\n", *controlReg);
    CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION, "Encryption done in %u us",
                      microSecsTaken);

    uint8 cyphertext[AES_BLOCK_SIZE] = {0};
    memcpy((void *)cyphertext, (void *)cyphertextReg, AES_BLOCK_SIZE);

    char cyphertextHexString[2 * AES_BLOCK_SIZE + 1] = {'\0'}; // 2 hex digits per byte + null terminator
    FPGA_CTRL_BinToHexStr(cyphertextHexString, (void *)cyphertext, AES_BLOCK_SIZE);
    CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION, "Cyphertext: %s", cyphertextHexString);
    memcpy(globalState.cyphertextHexString, cyphertextHexString, sizeof(globalState.cyphertextHexString));

    if ((err = mmio_lib_DeleteMapping((void *)controlReg, MAP_RANGE)) < CFE_SUCCESS)
        return err;
    if ((err = mmio_lib_DeleteMapping((void *)inBlk, MAP_RANGE)) < CFE_SUCCESS)
        return err;
    if ((err = mmio_lib_DeleteMapping((void *)outBlk, MAP_RANGE)) < CFE_SUCCESS)
        return err;

    return CFE_SUCCESS;
#undef AES_BLOCK_SIZE
}

static void FPGA_CTRL_BinToHexStr(char *const hexStrBuf, uint8 const *const bin, cpusize const binLen)
{
    memset(hexStrBuf, '\0', 2 * binLen + 1); // 2 hex digits per byte + null terminator
    for (cpusize i = 0; i < binLen; ++i)
    {
        snprintf(hexStrBuf + i * 2, 3, "%02x", bin[i]);
    }
}
