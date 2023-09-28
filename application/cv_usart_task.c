/**
 * @file       cv_usart_task.c
 * @brief      Computer vision communication
 * @arthur     2022 MacFalcons Control Team
 * All UART packets from CV should have the same size (DATA_PACKAGE_SIZE). Pad packet payloads with CHAR_UNUSED in the end if necessary.
 */

#pragma push
#pragma anon_unions
// Start of section using anonymous unions

#include "cv_usart_task.h"
#include "main.h"
#include "cmsis_os.h"

#include "usart.h"
#include "bsp_usart.h"
#include "string.h"
#include "detect_task.h"
#if defined(DEBUG_CV)
#include "usb_task.h"
#include <stdio.h>
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif /* MIN */
#define DATA_PACKAGE_SIZE 21
#define DATA_PACKAGE_HEADER_SIZE 2
#define DATA_PACKAGE_HEADLESS_SIZE (DATA_PACKAGE_SIZE - DATA_PACKAGE_HEADER_SIZE)
#define DATA_PACKAGE_PAYLOAD_SIZE (DATA_PACKAGE_HEADLESS_SIZE - sizeof(uint16_t) - sizeof(uint8_t)) // sizeof(uiTimestamp) and sizeof(bMsgType)
#define CHAR_UNUSED 0xFF
#define SHOOT_TIMEOUT_SEC 2000
#define CV_TRANDELTA_FILTER_SIZE 4 // TranDelta means Transmission delay

// Test result with pyserial: Message burst is at max 63 bytes per time, so any number bigger than 63 is fine for Rx buffer size
uint8_t abUsartRxBuf[DATA_PACKAGE_SIZE * 3];

#if defined(CV_INTERFACE)

typedef enum
{
    MSG_MODE_CONTROL = 0x10,
    MSG_CV_CMD = 0x20,
    MSG_ACK = 0x40,
    MSG_INFO_REQUEST = 0x50,
    MSG_INFO_FEEDBACK = 0x51,
} eMsgTypes;

typedef enum
{
    CV_INFO_TRANDELTA_BIT = 1 << 0,
    CV_INFO_CVSYNCTIME_BIT = 1 << 1,
    CV_INFO_LAST_BIT = 1 << 2,
} eInfoBits;
STATIC_ASSERT(CV_INFO_LAST_BIT <= (1 << 8));

typedef struct __attribute__((packed))
{
    uint8_t abAckAscii[3];
    uint16_t uiReqTimestamp; ///< corresponding request timestamp
    uint16_t uiExecDelta;    ///< execution time of request
    uint16_t uiCvSyncTime;   ///< native synchronization time of CV
    uint8_t abUnusedPayload[];
} tCvAckMsgPayload;
STATIC_ASSERT(sizeof(tCvAckMsgPayload) <= DATA_PACKAGE_PAYLOAD_SIZE);

typedef union __attribute__((packed))
{
    struct __attribute__((packed))
    {
        uint8_t abMessageHeader[DATA_PACKAGE_HEADER_SIZE]; ///< always '>>'
        uint16_t uiTimestamp;
        uint8_t bMsgType;
        union __attribute__((packed))
        {
            uint8_t abPayload[DATA_PACKAGE_PAYLOAD_SIZE];
            tCvAckMsgPayload CvAckMsgPayload;
        };
    } tData;
    uint8_t abData[DATA_PACKAGE_SIZE];
} tCvMsg;

typedef struct
{
    moving_average_type_t TranDeltaFilter;
    fp32 adTranDeltaFilterBuffer[CV_TRANDELTA_FILTER_SIZE];
    int16_t iTranDeltaMA;
    uint16_t uiCtrlSyncTime;
    uint16_t uiCvSyncTime;
} tCvTimestamps;

void CvCmder_Init(void);
void CvCmder_PollForModeChange(void);
void CvCmder_RxParser(void);
void CvCmder_EchoTxMsgToUsb(void);
void CvCmder_SendSetModeRequest(void);
void CvCmder_SendInfoFeedback(eInfoBits InfoBit);
void CvCmder_UpdateTranDelta(void);
void CvCmder_ChangeMode(uint8_t bCvModeBit, uint8_t fFlag);
#if defined(DEBUG_CV)
uint8_t CvCmder_MockModeChange(void);
#endif

tCvMsg CvRxBuffer;
tCvMsg CvTxBuffer;
tCvCmdHandler CvCmdHandler;
// don't compare with literal string "ACK", since it contains extra NULL char at the end
const uint8_t abExpectedAckPayload[3] = {'A', 'C', 'K'};
const uint8_t abExpectedMessageHeader[DATA_PACKAGE_HEADER_SIZE] = {'>', '>'};
uint8_t abExpectedUnusedPayload[DATA_PACKAGE_PAYLOAD_SIZE];
tCvTimestamps CvTimestamps;

#if defined(DEBUG_CV)
uint8_t fIsUserKeyPressingEdge = 0;
char usbMsg[500];
volatile uint16_t uiUsbMsgSize;
#endif

void cv_usart_task(void const *argument)
{
    CvCmder_Init();
    while (1)
    {
        CvCmder_PollForModeChange();
        // shoot mode timeout logic
        if (CvCmder_GetMode(CV_MODE_SHOOT_BIT) && (HAL_GetTick() - CvCmdHandler.ulShootStartTime > SHOOT_TIMEOUT_SEC))
        {
            CvCmder_ChangeMode(CV_MODE_SHOOT_BIT, 0);
        }
        osDelay(1500);
    }
}

void CvCmder_Init(void)
{
    CvTimestamps.TranDeltaFilter.size = CV_TRANDELTA_FILTER_SIZE;
    CvTimestamps.TranDeltaFilter.cursor = 0;
    CvTimestamps.TranDeltaFilter.ring = CvTimestamps.adTranDeltaFilterBuffer;
    CvTimestamps.TranDeltaFilter.sum = 0;
    CvTimestamps.uiCtrlSyncTime = 0;

    memcpy(CvRxBuffer.tData.abMessageHeader, abExpectedMessageHeader, sizeof(abExpectedMessageHeader));
    memcpy(CvTxBuffer.tData.abMessageHeader, abExpectedMessageHeader, sizeof(abExpectedMessageHeader));
    memset(abExpectedUnusedPayload, CHAR_UNUSED, sizeof(abExpectedUnusedPayload));

    memset(&CvCmdHandler, 0, sizeof(CvCmdHandler));       // clear status
    CvCmdHandler.cv_rc_ctrl = get_remote_control_point(); // reserved, not used yet

#if !defined(SENTRY_HW_TEST) && defined(SENTRY_1)
    CvCmder_ChangeMode(CV_MODE_AUTO_MOVE_BIT, 1);
    CvCmdHandler.fIsModeChanged = 1;
#endif

    // Get a callback when DMA completes or IDLE
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, abUsartRxBuf, sizeof(abUsartRxBuf));
    // disable half transfer interrupt, because if it coincide with IDLE or DMA interrupt, callback will be called twice
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    // RXNE is not used
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
}

/**
 * @brief if a command is received from remote controller, we keep sending set-mode requests to CV until an ACK is received
 */
void CvCmder_PollForModeChange(void)
{
    // @TODO: Implement check for Auto-move mode and Enemy mode
    // uint8_t fLastEnemyMode = CvCmder_GetMode(CV_MODE_ENEMY_DETECTED_BIT);
    static enum {
        CMDER_STATE_IDLE,
        CMDER_STATE_WAIT_FOR_ACK,
    } eCvCmderState = CMDER_STATE_IDLE;

    switch (eCvCmderState)
    {
    case CMDER_STATE_IDLE:
    {
#if defined(DEBUG_CV)
        if (CvCmder_CheckAndResetFlag(&CvCmdHandler.fIsModeChanged) || toe_is_error(CV_TOE) || CvCmder_MockModeChange())
#else
        if (CvCmder_CheckAndResetFlag(&CvCmdHandler.fIsModeChanged) || toe_is_error(CV_TOE))
#endif
        {
            CvCmder_SendSetModeRequest();
            CvCmdHandler.fIsWaitingForAck = 1;
            eCvCmderState = CMDER_STATE_WAIT_FOR_ACK;
        }
        break;
    }
    case CMDER_STATE_WAIT_FOR_ACK:
    {
        if (CvCmdHandler.fIsWaitingForAck)
        {
            CvCmder_SendSetModeRequest();
            // reset receive interrupt to detect new UART connection, in case CV boots up after control
            HAL_UARTEx_ReceiveToIdle_DMA(&huart1, abUsartRxBuf, sizeof(abUsartRxBuf));
            __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
        }
        else
        {
            eCvCmderState = CMDER_STATE_IDLE;
        }
        break;
    }
    default:
    {
        eCvCmderState = CMDER_STATE_IDLE;
        break;
    }
    }
}

void CvCmder_DetectAutoAimSwitchEdge(uint8_t fIsKeyPressed)
{
    // no need to debounce because keyboard signal is clean
    static uint8_t fLastKeySignal = 0;
    if (fLastKeySignal != fIsKeyPressed)
    {
        if (fIsKeyPressed)
        {
            // keyboard "G" button toggles auto-aim mode
            CvCmder_ChangeMode(CV_MODE_AUTO_AIM_BIT, CvCmder_GetMode(CV_MODE_AUTO_AIM_BIT) ? 0 : 1);
            CvCmdHandler.fIsModeChanged = 1;
        }
        fLastKeySignal = fIsKeyPressed;
    }
}

void CvCmder_EchoTxMsgToUsb(void)
{
#if defined(DEBUG_CV)
    // echo to usb
    // watch out of null character at the end
    memcpy(usbMsg, "Sent: ", sizeof("Sent: ") - 1);
    uiUsbMsgSize = sizeof("Sent: ") - 1;
    for (uint8_t i = 0; i < sizeof(CvTxBuffer.abData); i++)
    {
        if (uiUsbMsgSize + (sizeof("0x00,") - 1) > sizeof(usbMsg))
        {
            break;
        }
        uiUsbMsgSize += snprintf(&usbMsg[uiUsbMsgSize], sizeof(usbMsg) - uiUsbMsgSize, "0x%02X,", CvTxBuffer.abData[i]);
    }
    usbMsg[uiUsbMsgSize - 1] = '\n';
    usbMsg[uiUsbMsgSize] = 0;
    usb_printf("%s", usbMsg);
#endif
}

void CvCmder_SendSetModeRequest(void)
{
    CvTxBuffer.tData.bMsgType = MSG_MODE_CONTROL;
    // If current timestamp is smaller than sync_time, add 0x10000 to it. It's automatically handled by uint16_t type
    CvTxBuffer.tData.uiTimestamp = (uint16_t)xTaskGetTickCount() - CvTimestamps.uiCtrlSyncTime;
    memset(CvTxBuffer.tData.abPayload, CHAR_UNUSED, DATA_PACKAGE_PAYLOAD_SIZE);
    CvTxBuffer.tData.abPayload[0] = CvCmdHandler.fCvMode;
    HAL_UART_Transmit(&huart1, CvTxBuffer.abData, sizeof(CvTxBuffer.abData), 100);

    CvCmdHandler.fCvCmdValid = CvCmdHandler.fCvCmdValid && (CvCmder_GetMode(CV_MODE_AUTO_MOVE_BIT) || CvCmder_GetMode(CV_MODE_AUTO_AIM_BIT));
    CvCmder_EchoTxMsgToUsb();
}

void CvCmder_SendInfoFeedback(eInfoBits InfoBit)
{
    CvTxBuffer.tData.bMsgType = MSG_INFO_FEEDBACK;
    // If current timestamp is smaller than sync_time, add 0x10000 to it. It's automatically handled by uint16_t type
    CvTxBuffer.tData.uiTimestamp = (uint16_t)xTaskGetTickCount() - CvTimestamps.uiCtrlSyncTime;
    memset(CvTxBuffer.tData.abPayload, CHAR_UNUSED, DATA_PACKAGE_PAYLOAD_SIZE);
    CvTxBuffer.tData.abPayload[0] = InfoBit;
    switch (InfoBit)
    {
    case CV_INFO_TRANDELTA_BIT:
    {
        memcpy(&CvTxBuffer.tData.abPayload[1], &CvTimestamps.iTranDeltaMA, sizeof(CvTimestamps.iTranDeltaMA));
        break;
    }
    case CV_INFO_CVSYNCTIME_BIT:
    {
        memcpy(&CvTxBuffer.tData.abPayload[1], &CvTimestamps.uiCvSyncTime, sizeof(CvTimestamps.uiCvSyncTime));
        break;
    }
    default:
    {
        // should not reach here
        return;
    }
    }
    HAL_UART_Transmit(&huart1, CvTxBuffer.abData, sizeof(CvTxBuffer.abData), 100);
    CvCmder_EchoTxMsgToUsb();
}

void CvCmder_RxParser(void)
{
    uint8_t fValid = 0;
    switch (CvRxBuffer.tData.bMsgType)
    {
    case MSG_CV_CMD:
    {
        fValid = (CvCmder_GetMode(CV_MODE_AUTO_MOVE_BIT) || CvCmder_GetMode(CV_MODE_AUTO_AIM_BIT));
        // // Check for ACK is not used, because it may be misaligned in the middle and ignored so that further msgs align
        // // However, ACK is still helpful because it can help synchronize communication in the beginning
        // fValid &= CvCmdHandler.fIsWaitingForAck;
        fValid &= (memcmp(&CvRxBuffer.tData.abPayload[sizeof(tCvCmdMsg)], abExpectedUnusedPayload, DATA_PACKAGE_PAYLOAD_SIZE - sizeof(tCvCmdMsg)) == 0);
        if (fValid)
        {
            CvCmder_UpdateTranDelta();
            memcpy(&(CvCmdHandler.CvCmdMsg), CvRxBuffer.tData.abPayload, sizeof(CvCmdHandler.CvCmdMsg));
            CvCmdHandler.fCvCmdValid = 1;
        }
        else
        {
            CvCmdHandler.fCvCmdValid = 0;
            // if messages have been invalid for too long (offline timeout value is defined in detect_init() function)
            if (toe_is_error(CV_TOE))
            {
                memset(&(CvCmdHandler.CvCmdMsg), 0, sizeof(CvCmdHandler.CvCmdMsg));
            }
        }
        break;
    }
    case MSG_ACK:
    {
        fValid = (memcmp(CvRxBuffer.tData.CvAckMsgPayload.abUnusedPayload, abExpectedUnusedPayload, DATA_PACKAGE_PAYLOAD_SIZE - sizeof(tCvAckMsgPayload)) == 0);
        fValid &= (memcmp(CvRxBuffer.tData.CvAckMsgPayload.abAckAscii, abExpectedAckPayload, sizeof(abExpectedAckPayload)) == 0);
        if (fValid)
        {
            // Synchronize time after ACK
            uint16_t uiCtrlTimestamp = xTaskGetTickCount();
            int16_t iTranDelta = ((uiCtrlTimestamp - CvTimestamps.uiCtrlSyncTime) - CvRxBuffer.tData.CvAckMsgPayload.uiReqTimestamp - CvRxBuffer.tData.CvAckMsgPayload.uiExecDelta) / 2;
            CvTimestamps.iTranDeltaMA = moving_average_calc(iTranDelta, &CvTimestamps.TranDeltaFilter, (CvTimestamps.uiCtrlSyncTime == 0) ? MOVING_AVERAGE_RESET : MOVING_AVERAGE_CALC);
            CvTimestamps.uiCtrlSyncTime = uiCtrlTimestamp - iTranDelta;
            CvTimestamps.uiCvSyncTime = CvRxBuffer.tData.CvAckMsgPayload.uiCvSyncTime;

            CvCmdHandler.fIsWaitingForAck = 0;
        }
        break;
    }
    case MSG_MODE_CONTROL:
    {
        // Mode bits in set-mode message cv should be all zeros except for CV_MODE_SHOOT_BIT, which is controlled by cv
        fValid = ((CvRxBuffer.tData.abPayload[0] & (~(CV_MODE_LAST_BIT - 1))) == 0);
        fValid &= (memcmp(&CvRxBuffer.tData.abPayload[1], abExpectedUnusedPayload, DATA_PACKAGE_PAYLOAD_SIZE - 1) == 0);
        if (fValid)
        {
            CvCmder_UpdateTranDelta();
            CvCmder_ChangeMode(CV_MODE_SHOOT_BIT, CvRxBuffer.tData.abPayload[0] & CV_MODE_SHOOT_BIT);
            CvCmdHandler.ulShootStartTime = HAL_GetTick();
        }
        else
        {
            CvCmder_ChangeMode(CV_MODE_SHOOT_BIT, 0);
        }
        break;
    }
    case MSG_INFO_REQUEST:
    {
        fValid = ((CvRxBuffer.tData.abPayload[0] & (~(CV_INFO_LAST_BIT - 1))) == 0);
        fValid &= (memcmp(&CvRxBuffer.tData.abPayload[1], abExpectedUnusedPayload, DATA_PACKAGE_PAYLOAD_SIZE - 1) == 0);
        if (fValid)
        {
            CvCmder_UpdateTranDelta();
            uint8_t bInfoTestBit = 1;
            while (bInfoTestBit < CV_INFO_LAST_BIT)
            {
                if (CvRxBuffer.tData.abPayload[0] & bInfoTestBit)
                {
                    CvCmder_SendInfoFeedback((eInfoBits)bInfoTestBit);
                }
                bInfoTestBit = bInfoTestBit << 1;
            }
        }
        break;
    }
    }

    if (fValid)
    {
        detect_hook(CV_TOE);
    }

#if defined(DEBUG_CV)
    // echo to usb
    uiUsbMsgSize = snprintf(usbMsg, sizeof(usbMsg), "Received: (%s) ", fValid ? "Valid" : "Invalid");

    for (uint16_t i = 0; i < sizeof(CvRxBuffer.abData); i++)
    {
        if (uiUsbMsgSize + (sizeof("0x00,") - 1) > sizeof(usbMsg))
        {
            break;
        }
        uiUsbMsgSize += snprintf(&usbMsg[uiUsbMsgSize], sizeof(usbMsg) - uiUsbMsgSize, "0x%02X,", CvRxBuffer.abData[i]);
    }
    usbMsg[uiUsbMsgSize - 1] = '\n';
    usbMsg[uiUsbMsgSize] = 0;
    usb_printf("%s", usbMsg);
#endif
}

void CvCmder_UpdateTranDelta(void)
{
    int16_t iTranDelta = (uint16_t)xTaskGetTickCount() - CvTimestamps.uiCtrlSyncTime - CvRxBuffer.tData.uiTimestamp;
    CvTimestamps.iTranDeltaMA = moving_average_calc(iTranDelta, &CvTimestamps.TranDeltaFilter, MOVING_AVERAGE_CALC);
}

uint8_t CvCmder_GetMode(uint8_t bCvModeBit)
{
    return (CvCmdHandler.fCvMode & bCvModeBit);
}

void CvCmder_ChangeMode(uint8_t bCvModeBit, uint8_t fFlag)
{
    CvCmdHandler.fCvMode = (CvCmdHandler.fCvMode & ~bCvModeBit) | (fFlag ? bCvModeBit : 0);
}

/**
 * @brief Get pointer to Cv Cmd Handler
 * TODO: use it in gimbal_task and chassis_task
 */
tCvCmdHandler *CvCmder_GetHandler(void)
{
    return &CvCmdHandler;
}

uint8_t CvCmder_CheckAndResetFlag(uint8_t *pbFlag)
{
    uint8_t temp = *pbFlag;
    *pbFlag = 0;
    return temp;
}

#if defined(DEBUG_CV)
uint8_t CvCmder_CheckAndResetUserKeyEdge(void)
{
    return CvCmder_CheckAndResetFlag(&fIsUserKeyPressingEdge);
}

/**
 * @brief mock event of user enabling/disabling CV control mode by pressing button
 */
uint8_t CvCmder_MockModeChange(void)
{
    // polling for key press. We can't use interrupt because some other module interfere with it (maybe IMU DMA)
    GPIO_PinState fRead1;
    GPIO_PinState fRead2;
    fRead1 = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
    // debounce
    osDelay(20);
    fRead2 = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);

    if (fRead1 == fRead2)
    {
        static GPIO_PinState LastUserKeyState = GPIO_PIN_SET;
        fIsUserKeyPressingEdge = (fRead1 == GPIO_PIN_RESET) && (LastUserKeyState == GPIO_PIN_SET);
        LastUserKeyState = fRead1;

        if (fIsUserKeyPressingEdge)
        {
            static uint8_t bMockCounter = 0;
            switch (bMockCounter)
            {
            case 0:
            {
                CvCmdHandler.fCvMode = 0;
                break;
            }
            case 1:
            {
                CvCmdHandler.fCvMode = CV_MODE_ENEMY_DETECTED_BIT;
                break;
            }
            case 2:
            {
                CvCmdHandler.fCvMode = CV_MODE_AUTO_MOVE_BIT | CV_MODE_ENEMY_DETECTED_BIT;
                break;
            }
            case 3:
            {
                CvCmdHandler.fCvMode = CV_MODE_AUTO_AIM_BIT | CV_MODE_AUTO_MOVE_BIT | CV_MODE_ENEMY_DETECTED_BIT;
                break;
            }
            case 4:
            {
                CvCmdHandler.fCvMode = CV_MODE_AUTO_AIM_BIT | CV_MODE_AUTO_MOVE_BIT | CV_MODE_ENEMY_DETECTED_BIT | CV_MODE_SHOOT_BIT;
                break;
            }
            }
            bMockCounter = (bMockCounter + 1) % 5;
        }
    }
    return fIsUserKeyPressingEdge;
}
#endif // defined(DEBUG_CV)

#endif // defined(CV_INTERFACE)

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
#if defined(CV_INTERFACE)
        uint16_t uiHeaderFinder = 0;
        while ((uiHeaderFinder < Size) && (Size - uiHeaderFinder >= sizeof(CvRxBuffer.abData)))
        {
            if (abUsartRxBuf[uiHeaderFinder] == abExpectedMessageHeader[0])
            {
                if (abUsartRxBuf[uiHeaderFinder + 1] == abExpectedMessageHeader[1])
                {
                    uiHeaderFinder++;
                }
                else
                {
                    // Deem msg as valid even if the first ">" header is lost
                }
                // point to msg type
                uiHeaderFinder++;
                memcpy(&CvRxBuffer.abData[DATA_PACKAGE_HEADER_SIZE], &abUsartRxBuf[uiHeaderFinder], DATA_PACKAGE_HEADLESS_SIZE);
                CvCmder_RxParser();
                uiHeaderFinder += DATA_PACKAGE_HEADLESS_SIZE;
            }
            else
            {
                uiHeaderFinder++;
            }
        }
#endif
        // @TODO: change to circular buffer strategy for faster restart time of DMA
        /* start the DMA again */
        // Do not remove this, otherwise RTOS task will stuck for unkown reasons
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, abUsartRxBuf, sizeof(abUsartRxBuf));
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}

// End of section using anonymous unions
#pragma pop
