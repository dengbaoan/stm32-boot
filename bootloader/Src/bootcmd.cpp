/**
  ******************************************************************************
  * @file    bootcmd.cpp
  * @brief   ��boot�������
	* @Author  dengbaoan
	* @Email   645603192@qq.com
  * @Version v0.0.1
  * @Date:   2019.06.11
	* @attention
  ******************************************************************************
  */
#include "bootcmd.h"
#include "usart.h"
#include "crc.h"
#include "debug.h"
#include "bootloader.h"

#include <string.h>
#include <stdlib.h>

#define  APP_BASE_ADDRESS            (0x8004000)   //!< Ӧ�ó�����ʼ��ַ
#define  PRIVATE_DATA_BASE_ADDRESS   (0x801F000)  //!< ˽��������ʼ��ַ
#define  APP_OFFSET_MAX              (0x10000)     //!< Ӧ�ó����ַ�ռ����ƫ����
#define  PRIVATE_DATA_OFFSET_MAX     (0x1000)       //!< ˽�����ݵ�ַ�ռ����ƫ����
#define  UART_BUFF_SIZE              (2048)        //!< ���ڻ�������С���˴���Ҫ������λ�������ٶ��ʵ�������


#define  PRIVATE_APPSIZE_OFFSET      (0)           //!< ˽������ 4�ֽ�APP�̼����� �洢ƫ�Ƶ�ַ
#define  PRIVATE_CRC_OFFSET          (4)           //!< ˽������ 32λCRC          �洢ƫ�Ƶ�ַ


uint8_t g_uartRxBuff[UART_BUFF_SIZE] = {0};       //! ���ڽ��ջ�����
uint8_t g_bootBuff[UART_BUFF_SIZE]   = {0};       //! bootData������

volatile uint32_t  g_uart1RxFlag  = 0;            //! uart ���ݸ��±�־����HAL_UART_RxCpltCallback�����ӣ����û�����g_bootBuff���С��
volatile uint32_t  g_bootBuffSize = 0;            //! g_bootBuff��uart�и��µ���������


static void RunBootCmd(uint8_t cmd);
static void RunCmdUpdateApp();
static void RunCmdJumpToApp();

static void RunCmdConnection();
static void RunCmdEraseFlash();
static void RunCmdProgramFlash();
static void RunCmdWriteCrc();
static void RunCmdRestart();
static bool WritePrivateData(char *pBuf, int size, uint32_t offset);
static bool CrcVerify();
/**
  * @brief  Rx Transfer completed callbacks.
  * @param  huart: pointer to a UART_HandleTypeDef structure that contains
  *                the configuration information for the specified UART module.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /* Prevent unused argument(s) compilation warning */
    UNUSED(huart);
    static volatile uint32_t lastPosit = 0;

    int curPosit  = huart->RxXferSize - __HAL_DMA_GET_COUNTER(huart->hdmarx);
    //DEBUG("lastPosit: %d curPosit:%d ",lastPosit, curPosit);

    /*! bootbuf����ʹ�� ���� DMA����λ��û�б仯ʱֱ���˳�*/
    if( (g_uart1RxFlag > 0) || (curPosit == lastPosit) )
    {
        //ERROR("count: %d",  __HAL_DMA_GET_COUNTER(huart->hdmarx));
        return;
    }

    /*! ���bootbuf */
    g_bootBuffSize = 0;
    memset(g_bootBuff,0x00,sizeof(g_bootBuff)); //�������


    /*! ��DMAbuf��ת�����ݵ�bootbuf�� */
    if( curPosit > lastPosit)
    {
        int cpySize1 = curPosit - lastPosit;

        memcpy(g_bootBuff, huart->pRxBuffPtr + lastPosit, cpySize1);
        g_bootBuffSize = cpySize1;

    }
    else if( curPosit < lastPosit)
    {
        int cpySize1 = huart->RxXferSize - lastPosit;
        int cpySize2 = curPosit;

        memcpy(g_bootBuff,          huart->pRxBuffPtr + lastPosit, cpySize1);
        memcpy(g_bootBuff+cpySize1, huart->pRxBuffPtr,              cpySize2);
        g_bootBuffSize = cpySize1 + cpySize2;

    }
    lastPosit = curPosit;
    g_uart1RxFlag++;

    //DEBUG("--------- %d \n", g_uart1RxFlag);
    //DEBUG("%s\n", g_bootBuff);
}

/** @brief     boot��ѭ����
  * @note      ��ѭ��������bootcmdģʽ�ȴ��������룬
               ���3s������������,����Զ��bootcmdģʽ��
               ��������֤app��crc����֤ͨ��ȥִ��app����crc��ͨ�������bootcmdģʽ��
  */
void DoLoop()
{
    HAL_UART_Receive_DMA(&huart1, g_uartRxBuff, UART_BUFF_SIZE);

    bool bBootCmdLoop = false;
    while(1)
    {
        int  timeOut100ms = 30;
        while( (timeOut100ms--) > 0)
        {
            if( (g_uart1RxFlag > 0))
            {
                g_uart1RxFlag--;

                if((g_bootBuffSize >= 3))
                {
                    bBootCmdLoop = true;

                    if( (g_bootBuff[0] == 0xFE) && (g_bootBuff[1] == 0xA5) )
                    {
                        RunBootCmd(g_bootBuff[2]);
                    }
                }
            }

            HAL_GPIO_TogglePin(GPIOD, led1_Pin);
            HAL_Delay(100);
        }

        if( (!bBootCmdLoop) && CrcVerify())
        {
            RunCmdJumpToApp();
        }
    }
}

/** @brief     boot cmd����ִ�к���
  * @param[in] cmd : ����
  */
static void RunBootCmd(uint8_t cmd)
{
    switch(cmd)
    {
    case 0x01:
        RunCmdConnection();
        break;
    case 0x02:
        RunCmdEraseFlash();
        break;
    case 0x04:
        RunCmdProgramFlash();
        break;
    case 0x05:
        RunCmdWriteCrc();
        break;
    case 0xF2:
        RunCmdRestart();
        break;
    case 0xF3:
        RunCmdJumpToApp();
        break;
    case 0xF4:
    {
        if(CrcVerify())
        {
            RunCmdJumpToApp();
        }
    }
    break;
    case 0xF5:
        RunCmdUpdateApp();
        break;
    default :
        break;
    }
}

/** @brief     �������ֺ���
  * @note      û��ʵ�����ã������Ƕ�cmd��һ���ض�Ӧ��������֮ǰ��Ҫȷ����Ƭ���Ѿ��ڹ�����
  */
static void RunCmdConnection()
{
    DEBUG("---------------Connection ---------");
    uint8_t temp[] = {0XFE, 0XA5, 0X01, 0X03, 0XFF, 0X03, 0X00, 0X01};
    HAL_UART_Transmit(&huart1, temp, sizeof(temp)/sizeof(temp[0]), 0xFFFF);
}

/** @brief     flash��������
  * @note      ��app����������в�����
  */
static void RunCmdEraseFlash()
{
    DEBUG("--------------- Erase Flash ---------");
    FlashErase(APP_BASE_ADDRESS, APP_BASE_ADDRESS + APP_OFFSET_MAX);
}

/** @brief     ��flash���
  * @note      �����ڽ��յ�������д�뵽flash�У�ֱ������0xFE 0xA5 0xF1ֹͣ��
  * @note      ������̺󣬻Ὣ���α�̳���д��˽���������� ��ַ��PRIVATE_DATA_BASE_ADDRESS + PRIVATE_APPSIZE_OFFSET��
  * @warning   ���app��bin�ļ����� 0xFE 0xA5 0xF1����ᵼ������д�벻������
  */
static void RunCmdProgramFlash()
{
    uint32_t startAddr = APP_BASE_ADDRESS;

    DEBUG("--------------- write Flash---------");
    g_uart1RxFlag = 0;
    uint32_t  count                 = 0;
    const int alignWidth            = 8;
    uint8_t   alignBuff[alignWidth] = {0};
    int       alignBuffOffset       = 0;

    while(1)
    {
        if(g_uart1RxFlag > 0)
        {
            bool bQuit     = false;
            /*! Exit programming */
            if(g_bootBuff[g_bootBuffSize-3] == 0xFE  &&
                    g_bootBuff[g_bootBuffSize-2] == 0xA5  &&
                    g_bootBuff[g_bootBuffSize-1] == 0xF1  )
            {
                bQuit = true;
                g_bootBuffSize -= 3;
                DEBUG("----------------g_bootBuffSize:%d ----------------------------\n", g_bootBuffSize);
            }

            int  preSize   =  ((alignWidth - alignBuffOffset) > g_bootBuffSize) ? g_bootBuffSize : (alignWidth - alignBuffOffset);
            int  postSize  =  (g_bootBuffSize - preSize) % alignWidth;
            int  writeSize = 0;

            /*! preorder */
            memcpy( alignBuff + alignBuffOffset, g_bootBuff, preSize);

            writeSize = alignBuffOffset + preSize;
            FlashWrite( (char *)alignBuff, startAddr + count, writeSize);
            count += writeSize;

            /*! inorder */
            writeSize = g_bootBuffSize - preSize - postSize;
            FlashWrite( ((char *)g_bootBuff) + preSize, startAddr + count, writeSize);
            count += writeSize;

            /*! postorder*/
            memcpy( alignBuff, g_bootBuff + (g_bootBuffSize - postSize), postSize);
            alignBuffOffset = postSize;

            if(bQuit)
            {
                FlashWrite( (char *)alignBuff, startAddr + count, alignBuffOffset);
                count += alignBuffOffset;
                DEBUG("----------------write finish :%d ----------------------------\n", count);
							  HAL_GPIO_TogglePin(GPIOC, led3_Pin);
                break;
            }
            DEBUG("----------------count:%d  buf size:%d  flag:%d  alignBuffOffset:%d \n", count, g_bootBuffSize, g_uart1RxFlag, alignBuffOffset);
						
            HAL_GPIO_TogglePin(GPIOC, led2_Pin);
            g_uart1RxFlag--;
        }
    }

    /*! ��¼app�̼����ȣ�д��˽����������*/
    WritePrivateData( (char *)(&count), sizeof(count), PRIVATE_APPSIZE_OFFSET);
}

/** @brief     д��app�����32λcrcУ����
  * @note      У����Ӵ��ڽ��գ������ֽڣ����յ�4�ֽ�ǰ��һֱ�ȴ���
  */
static void RunCmdWriteCrc()
{
    do
    {
        if( g_uart1RxFlag > 0 )
        {
            g_uart1RxFlag--;
            if(g_bootBuffSize >= 4)
            {
                uint32_t crcValue = 0;
                memcpy((uint8_t*)(&crcValue), g_bootBuff, 4);
                DEBUG("----------------Write crc:%04x ----------------------------\n", crcValue);
                WritePrivateData( (char *)(&crcValue), sizeof(crcValue), PRIVATE_CRC_OFFSET);
                return;
            }
        }
    }
    while(1);
}

/** @brief     ��Ƭ��������
  */
static void RunCmdRestart()
{
    DEBUG("----------------Restart ----------------------------\n");
    HAL_NVIC_SystemReset();
}

/** @brief     д��˽������
  * @note      ˽�������ڴ������flash�С�
  * @warning   flash���Ե���ÿ��д���������������ݶ��������˽�������������²�����
               ����һ��Ҫ�滮������д���offset�����⽫�������ݸ��ǡ�
  */
static bool WritePrivateData(char *pBuf, int size, uint32_t offset)
{
    if( (offset + size) > PRIVATE_DATA_OFFSET_MAX)
    {
        ERROR("offset: %d size: %d",offset, size);
        return false;
    }

    bool bRet = true;

    char *pTempBuff = (char *)malloc(PRIVATE_DATA_OFFSET_MAX);
    assert(pTempBuff != NULL);

    pTempBuff = (char*)memset(pTempBuff, 0, PRIVATE_DATA_OFFSET_MAX);
    assert(pTempBuff != NULL);

    /*! step 1 ������ȫ����ȡ��buf*/
    FlashRead( pTempBuff, PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_OFFSET_MAX);

    /*! step 2 ����˽������*/
    FlashErase(PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_BASE_ADDRESS + PRIVATE_DATA_OFFSET_MAX);

    /*! step 3 �޸�����*/
    memcpy(pTempBuff + offset, pBuf, size);

    /*! step 4 ������һ����д��*/
    if (PRIVATE_DATA_OFFSET_MAX != FlashWrite( pTempBuff, PRIVATE_DATA_BASE_ADDRESS, PRIVATE_DATA_OFFSET_MAX) )
    {
        ERROR("Write private data.");
        bRet = false;
    }

    free(pTempBuff);
    pTempBuff = NULL;

    return bRet;
}

/** @brief     ʹ��stm32��Ӳ��crcУ��
  * @note      ����app���������crc����˽�������е�crc�Ա�
  * @warning   stm32��Ӳ��crc��32λ��̫��У�飬������crc������ͬ��
  */
static bool CrcVerify()
{
    uint32_t appLen = 0;
    FlashRead((char*)(&appLen), PRIVATE_DATA_BASE_ADDRESS + PRIVATE_APPSIZE_OFFSET, 4 );
    DEBUG("applen: %d", appLen);

    uint32_t appCrc = 0;
    FlashRead((char*)(&appCrc), PRIVATE_DATA_BASE_ADDRESS + PRIVATE_CRC_OFFSET, 4 );
    DEBUG("appcrc: 0x%04x", appCrc);

    uint32_t crcValue1 = 0;
    if(appLen <= APP_OFFSET_MAX)
    {
        crcValue1 = HAL_CRC_Calculate(&hcrc, (uint32_t *)APP_BASE_ADDRESS, appLen/4);
        DEBUG("crcvalue: 0x%04x ", crcValue1);
    }
    else
    {
        return false;
    }


    return (appCrc == crcValue1)? true : false;
}

/** @brief     ��ת��appִ��
  */
static void RunCmdJumpToApp()
{
#if 1
    const int readSize = 2048;
    char *pTempBuff = (char *)malloc(readSize);
    assert(pTempBuff != NULL);

    pTempBuff =(char*)memset(pTempBuff, 0, readSize);
    assert(pTempBuff != NULL);

    FlashRead((char*)pTempBuff, APP_BASE_ADDRESS, readSize);

    free(pTempBuff);
    pTempBuff = NULL;
#endif
    DEBUG("---------------Jump To App---------");

    /*! �ر�һЩ�Ѿ��򿪵�����*/
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_MspDeInit(&huart1);

    //__set_PRIMASK(1);//�����ж�,��app�����¿�����
    __disable_irq();

    JumpToApp(APP_BASE_ADDRESS);
}

/** @brief     �ӹ�������������
  * @note      �Ӵ��ڽ���app����Ķ������ļ����ļ�������ɺ��Զ���ת��appִ�У�������crcУ�顣
  */
static void RunCmdUpdateApp()
{
    RunCmdEraseFlash();
    RunCmdProgramFlash();
    RunCmdJumpToApp();
}
