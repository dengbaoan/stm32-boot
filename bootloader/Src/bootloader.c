#include "bootloader.h"
#include <string.h>
#include <stdio.h>

#include "debug.h"

typedef  void (*pFunction)(void);

/** @brief     ִ��flash�е�ָ��λ�õĴ��롣
  * @param[in] addr : flash��ַ��
  * @warning   ��תǰ����ʹ��δ�ض����printf��������������BEAB��λ�á� 
  */
void JumpToApp(uint32_t addr)
{
	//DEBUG("%04X", addr);
  if ( ((*(__IO uint32_t *)addr) & 0x2FFE0000 ) == 0x20000000)
  {
    /*�� Ӧ�ó�����ʼ��ַƫ��4��Ϊreset�жϷ�������ַ*/
    uint32_t  jumpAddress       = *(uint32_t*) (addr + 4);
		
		/*�� ��reset�жϷ�������ַת��Ϊ����ָ��*/
    pFunction JumpToApplication = (pFunction) jumpAddress;            
		
    /* Initialize user application's Stack Pointer */
		__set_PSP(*(volatile unsigned int*) addr);
		__set_CONTROL(0);
		__set_MSP(*(volatile unsigned int*) addr);
		
    JumpToApplication();
  }
	else
	{
    ERROR("not find APPlication...");
  }
}


/** @brief     ��flash�ж�ȡ���ݡ�
  * @param[in] pBuf : ���ݱ����ַ
  * @param[in] addr : flash��ʼ��ַ
  * @param[in] size : ��ȡ����
  * @return    �ɹ�����pBuf�е���������  
  */
uint8_t FlashRead( char *pBuf, uint32_t addr, int size)
{
	memcpy(pBuf, (char*)addr, size);
	
 	
	DEBUG("-----------------FlashRead--------------------");
	for(int i = 0; i< size; i++)
	{
		printf("%02X ", pBuf[i]);
		if( i % 16 == 0)
		{
			printf("\n");
		}
	}
	printf("\n");
/*!*/	
  return size;
}

/** @brief     ��flash��д�����ݡ�
  * @param[in] pBuf : ���ݵ�ַ
  * @param[in] addr : flash��ʼ��ַ
  * @param[in] size : д������
  * @return    �ɹ�д������
  * @warning   ��ʼ��ַ������16bit���룬��д����밴16λ��ַ��ʼд�롣   
  */
uint8_t FlashWrite( char *pBuf, uint32_t addr, int size)
{
	if ( addr%2 != 0)
	{
	 ERROR(" **************************** addr: %04X", addr);
	 return 0;
	}
	 
	const    int      alignWidth = 8;
  const    uint32_t starAddr   = addr;
  volatile int      remainSize = size;
	volatile int      offset     = 0;
	 
	HAL_FLASH_Unlock();
	while(remainSize > 0)
	{
		uint64_t data = 0xFFFFFFFFFFFFFFFF;
		
    int bufCpySize =  (remainSize >= alignWidth)? alignWidth : remainSize; 
		memcpy(&data, pBuf + offset, bufCpySize);

		//DEBUG("starAddr: %04x   data: %04X %04X",starAddr, (uint32_t)(data>>32), (uint32_t)data);
		if(  HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, starAddr + offset, data) != HAL_OK)
		{
			ERROR(" **************************** error: %d", HAL_FLASH_GetError());
			ERROR(" **************************** starAddr: %04X  offset: %d ", starAddr, offset);
			break;
		}

		offset     += bufCpySize;
		remainSize -= bufCpySize;
	}
	HAL_FLASH_Lock();
	
	return offset;
}

/** @brief     ����flash��������ɺ�flash��Ϊ0XFF��
  * @param[in] startAddr : flash��ʼ��ַ
  * @param[in] endAddr   : flash������ַ
  * @param[in] size : д������
  * @return    status ����״̬���ɹ�����HAL_OK
  * @note      ������С��λFLASH_PAGE_SIZE���Ұ�FLASH_PAGE_SIZE���룬δ��FLASH_PAGE_SIZE��С�Ĳ�������    
  */
uint8_t FlashErase(uint32_t startAddr, uint32_t endAddr)
{
		HAL_FLASH_Unlock();

		//��ʼ��FLASH_EraseInitTypeDef
		FLASH_EraseInitTypeDef f;
		f.TypeErase    =  FLASH_TYPEERASE_PAGES;
		f.PageAddress  =  startAddr;
		f.NbPages      = (endAddr - startAddr) / FLASH_PAGE_SIZE;
	
		//����PageError
		uint32_t           PageError = 0;
		HAL_StatusTypeDef  status = HAL_FLASHEx_Erase(&f, &PageError);
	
	  DEBUG("error:%d  status:%d\n", status, HAL_FLASH_GetError() );
	
		HAL_FLASH_Lock();  

		return status;
}


/*�� -----------------------------------------------------------------------------------------------------------------------------
---------------------------------------------------------------------------------------------------------------------------------*/
/** @brief flash��д���Ժ����� 
  */
void FlashTest(void)
{
	/*! read flash*/
	char readBuf[256]  = {0};
	char writeBuf[256] = {0};
	DEBUG("-----------------set buf--------------------");
	for(int i = 0; i< 255; i++)
	{
		writeBuf[i] = i;
		printf("%01X ", writeBuf[i]);
	}

	DEBUG("-----------------Read--------------------");
	FlashRead(readBuf, 0x8004000, 100);

	DEBUG("-----------------Erase--------------------");
	FlashErase(0x8004000, 0x800A000);
	FlashRead(readBuf, 0x8004000, 100);
	
	
#if 0	
	uint64_t data = 0x0123456789abcdef;
	
	if(  HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 0x8004001, data) != HAL_OK)
			{
					ERROR(" **************************** error: %d", HAL_FLASH_GetError());
				  printf("***************************** data: ");
				  char temp[8] = {0};
					memcpy( temp, &data, 8);
					for(int i=0; i<8; i++)
					{
						printf("%02X ", temp[i]);
					}
					printf("*******\n");
					//break;
			}
	  FlashRead(readBuf, 0x8004000, 20);
#else

	DEBUG("\n\n-----------------Write 1--------------------");
	FlashWrite(writeBuf, 0x8004001, 10);
	FlashRead(readBuf, 0x8004000, 100);

	DEBUG("\n\n-----------------Write 2--------------------");
	FlashWrite(writeBuf, 0x8004000, 10);
	FlashRead(readBuf, 0x8004000, 100);


	DEBUG("\n\n-----------------Write 3--------------------");
	FlashWrite(writeBuf, 0x8004000, 10);
	FlashRead(readBuf, 0x8004000, 100);
#endif

	printf("end\n");
}

