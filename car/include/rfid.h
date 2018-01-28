
/*
* File name：RFID.pde
* Creator：Dr.Leong   ( WWW.B2CQSHOP.COM )
* Creation date：2011.09.19
* Modified by： dima201246 (dima201246@gmail.com)
* Modified date： 2017.08.24
* Modified： Translation from Chinese to English (by google)
* Functional Description：Mifare1 Anti-collision find cards → → → Select card reader interface
*/

#include <SPI.h>

#define MAX_LEN 18	//Maximum length of the array

/////////////////////////////////////////////////////////////////////
//set the pin
/////////////////////////////////////////////////////////////////////
int chipSelectPin = 0;

//MF522 Command word
#define PCD_IDLE			0x00	//NO action; Cancel the current command
#define PCD_AUTHENT			0x0E	//Authentication Key
#define PCD_RECEIVE			0x08	//Receive Data
#define PCD_TRANSMIT		0x04	//Transmit data
#define PCD_TRANSCEIVE		0x0C	//Transmit and receive data,
#define PCD_RESETPHASE		0x0F	//Reset
#define PCD_CALCCRC			0x03	//CRC Calculate

// Mifare_One card command word
# define PICC_REQIDL		0x26	// find the antenna area does not enter hibernation
# define PICC_REQALL		0x52	// find all the cards antenna area
# define PICC_ANTICOLL		0x93	// anti-collision
# define PICC_SElECTTAG		0x93	// election card
# define PICC_AUTHENT1A		0x60	// authentication key A
# define PICC_AUTHENT1B		0x61	// authentication key B
# define PICC_READ			0x30	// Read Block
# define PICC_WRITE			0xA0	// write block
# define PICC_DECREMENT		0xC0	// debit
# define PICC_INCREMENT		0xC1	// recharge
# define PICC_RESTORE		0xC2	// transfer block data to the buffer
# define PICC_TRANSFER		0xB0	// save the data in the buffer
# define PICC_HALT			0x50	// Sleep


//And MF522 The error code is returned when communication
#define MI_OK				0
#define MI_NOTAGERR			1
#define MI_ERR				2


//------------------MFRC522 Register---------------
//Page 0:Command and Status
#define Reserved00			0x00
#define CommandReg			0x01
#define CommIEnReg			0x02
#define DivlEnReg			0x03
#define CommIrqReg			0x04
#define DivIrqReg			0x05
#define ErrorReg			0x06
#define Status1Reg			0x07
#define Status2Reg			0x08
#define FIFODataReg			0x09
#define FIFOLevelReg		0x0A
#define WaterLevelReg		0x0B
#define ControlReg			0x0C
#define BitFramingReg		0x0D
#define CollReg				0x0E
#define Reserved01			0x0F
//Page 1:Command
#define Reserved10			0x10
#define ModeReg				0x11
#define TxModeReg			0x12
#define RxModeReg			0x13
#define TxControlReg		0x14
#define TxAutoReg			0x15
#define TxSelReg			0x16
#define RxSelReg			0x17
#define RxThresholdReg		0x18
#define DemodReg			0x19
#define Reserved11			0x1A
#define Reserved12			0x1B
#define MifareReg			0x1C
#define Reserved13			0x1D
#define Reserved14			0x1E
#define SerialSpeedReg		0x1F
//Page 2:CFG
#define Reserved20			0x20
#define CRCResultRegM		0x21
#define CRCResultRegL		0x22
#define Reserved21			0x23
#define ModWidthReg			0x24
#define Reserved22			0x25
#define RFCfgReg			0x26
#define GsNReg				0x27
#define CWGsPReg			0x28
#define ModGsPReg			0x29
#define TModeReg			0x2A
#define TPrescalerReg		0x2B
#define TReloadRegH			0x2C
#define TReloadRegL			0x2D
#define TCounterValueRegH	0x2E
#define TCounterValueRegL	0x2F
//Page 3:TestRegister     
#define Reserved30			0x30
#define TestSel1Reg			0x31
#define TestSel2Reg			0x32
#define TestPinEnReg		0x33
#define TestPinValueReg		0x34
#define TestBusReg			0x35
#define AutoTestReg			0x36
#define VersionReg			0x37
#define AnalogTestReg		0x38
#define TestDAC1Reg			0x39  
#define TestDAC2Reg			0x3A   
#define TestADCReg			0x3B   
#define Reserved31			0x3C   
#define Reserved32			0x3D   
#define Reserved33			0x3E   
#define Reserved34			0x3F
//-----------------------------------------------

/*
* Function Name：Write_MFRC5200
* Function Description: To a certain MFRC522 register to write a byte of data
* Input Parameters：addr - register address; val - the value to be written
* Return value: None
*/

void Write_MFRC522(uint8_t addr, uint8_t val)
{
	digitalWrite(chipSelectPin, LOW);

	//Address Format：0XXXXXX0
	SPI.transfer((addr<<1)&0x7E); 
	SPI.transfer(val);
	
	digitalWrite(chipSelectPin, HIGH);
}

/*
* Function Name：Read_MFRC522
* Description: From a certain MFRC522 read a byte of data register
* Input Parameters: addr - register address
* Returns: a byte of data read from the
*/

uint8_t Read_MFRC522(uint8_t addr)
{
	uint8_t val;
	
	digitalWrite(chipSelectPin, LOW);

	//Address Format：1XXXXXX0
	SPI.transfer(((addr<<1)&0x7E) | 0x80);  
	val = SPI.transfer(0x00);

	digitalWrite(chipSelectPin, HIGH);

	return val; 
}

/*
* Function Name：SetBitMask
* Description: Set RC522 register bit
* Input parameters: reg - register address; mask - set value
* Return value: None
*/

void SetBitMask(uint8_t reg, uint8_t mask)  
{
	uint8_t tmp;
	tmp = Read_MFRC522(reg);
	Write_MFRC522(reg, tmp | mask);	// set bit mask
}

/*
* Function Name: ClearBitMask
* Description: clear RC522 register bit
* Input parameters: reg - register address; mask - clear bit value
* Return value: None
*/

void ClearBitMask(uint8_t reg, uint8_t mask)  
{
	uint8_t tmp;
	tmp = Read_MFRC522(reg);
	Write_MFRC522(reg, tmp & (~mask));	// clear bit mask
}

/*
* Function Name：AntennaOn
* Description: Open antennas, each time you start or shut down the natural barrier between the transmitter should be at least 1ms interval
* Input: None
* Return value: None
*/

void AntennaOn(void)
{
	uint8_t temp;

	temp = Read_MFRC522(TxControlReg);

	if (!(temp & 0x03))
	{
		SetBitMask(TxControlReg, 0x03);
	}
}

/*
* Function Name: AntennaOff
* Description: Close antennas, each time you start or shut down the natural barrier between the transmitter should be at least 1ms interval
* Input: None
* Return value: None
*/

void AntennaOff(void)
{
	ClearBitMask(TxControlReg, 0x03);
}

/*
* Function Name: ResetMFRC522
* Description: Reset RC522
* Input: None
* Return value: None
*/

void MFRC522_Reset(void)
{
	Write_MFRC522(CommandReg, PCD_RESETPHASE);
}

/*
* Function Name：InitMFRC522
* Description: Initialize RC522
* Input: None
* Return value: None
*/

void MFRC522_Init(void)
{
	MFRC522_Reset();

	//Timer: TPrescaler*TreloadVal/6.78MHz = 24ms
	Write_MFRC522(TModeReg, 0x8D);		//Tauto=1; f(Timer) = 6.78MHz/TPreScaler
	Write_MFRC522(TPrescalerReg, 0x3E);	//TModeReg[3..0] + TPrescalerReg
	Write_MFRC522(TReloadRegL, 30);
	Write_MFRC522(TReloadRegH, 0);

	Write_MFRC522(TxAutoReg, 0x40);	//100%ASK
	Write_MFRC522(ModeReg, 0x3D);	//CRC Initial value 0x6363  ???

	//ClearBitMask(Status2Reg, 0x08);	//MFCrypto1On=0
	//Write_MFRC522(RxSelReg, 0x86);	//RxWait = RxSelReg[5..0]
	//Write_MFRC522(RFCfgReg, 0x7F);	//RxGain = 48dB

	AntennaOn();	//Open the antenna
}

/*
* Function Name: MFRC522_ToCard
* Description: RC522 and ISO14443 card communication
* Input Parameters: command - MF522 command word,
*       sendData--RC522 sent to the card by the data
*       sendLen--Length of data sent  
*       backData--Received the card returns data,
*       backLen--Return data bit length
* Return value: the successful return MI_OK
*/

uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen)
{
	uint8_t status = MI_ERR;
	uint8_t irqEn = 0x00;
	uint8_t waitIRq = 0x00;
	uint8_t lastBits;
	uint8_t n;
	uint16_t i;

	switch (command)
	{
		case PCD_AUTHENT:	//Certification cards close
						{
							irqEn = 0x12;
							waitIRq = 0x10;
							break;
						}

		case PCD_TRANSCEIVE:	//Transmit FIFO data
						{
							irqEn = 0x77;
							waitIRq = 0x30;
							break;
						}
		default: break;
	}

	Write_MFRC522(CommIEnReg, irqEn|0x80);	//Interrupt request
	ClearBitMask(CommIrqReg, 0x80);			//Clear all interrupt request bit
	SetBitMask(FIFOLevelReg, 0x80);			//FlushBuffer=1, FIFO Initialization
		
	Write_MFRC522(CommandReg, PCD_IDLE);	//NO action; Cancel the current command???

	//Writing data to the FIFO
	for (i = 0; i < sendLen; i++)
	{
		Write_MFRC522(FIFODataReg, sendData[i]);    
	}

	//Execute the command
	Write_MFRC522(CommandReg, command);

	if (command == PCD_TRANSCEIVE)
	{
		SetBitMask(BitFramingReg, 0x80);    //StartSend=1,transmission of data starts  
	}

	//Waiting to receive data to complete
	i = 2000; //i according to the clock frequency adjustment, the operator M1 card maximum waiting time 25ms???
	do 
	{
		//CommIrqReg[7..0]
		//Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
		n = Read_MFRC522(CommIrqReg);
		i--;
	}

	while ((i!=0) && !(n&0x01) && !(n&waitIRq));

	ClearBitMask(BitFramingReg, 0x80);	//StartSend=0

	if (i != 0)
	{
		if(!(Read_MFRC522(ErrorReg) & 0x1B))  //BufferOvfl Collerr CRCErr ProtecolErr
		{
			status = MI_OK;

			if (n & irqEn & 0x01)
			{
				status = MI_NOTAGERR;	//??
			}

			if (command == PCD_TRANSCEIVE)
			{
				n = Read_MFRC522(FIFOLevelReg);
				lastBits = Read_MFRC522(ControlReg) & 0x07;

				if (lastBits)
				{
					*backLen = (n-1)*8 + lastBits;
				}
				else
				{
					*backLen = n*8;
				}

				if (n == 0)
				{
					n = 1;
				}

				if (n > MAX_LEN)
				{
					n = MAX_LEN;
				}
				
				//Reading the received data in FIFO
				for (i = 0; i < n; i++)
				{
					backData[i] = Read_MFRC522(FIFODataReg);    
				}
			}
		}
		else
		{
			status = MI_ERR; 
		}
	}
	
	//SetBitMask(ControlReg,0x80);	//timer stops
	//Write_MFRC522(CommandReg, PCD_IDLE); 

	return status;
}

/*
* Function Name：MFRC522_Request
* Description: Find cards, read the card type number
* Input parameters: reqMode - find cards way
*       TagType - Return Card Type
*        0x4400 = Mifare_UltraLight
*        0x0400 = Mifare_One(S50)
*        0x0200 = Mifare_One(S70)
*        0x0800 = Mifare_Pro(X)
*        0x4403 = Mifare_DESFire
* Return value: the successful return MI_OK
*/

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType)
{
	uint8_t status;  
	uint16_t backBits;	//The received data bits

	Write_MFRC522(BitFramingReg, 0x07);	//TxLastBists = BitFramingReg[2..0] ???

	TagType[0] = reqMode;
	status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

	if ((status != MI_OK) || (backBits != 0x10))
	{
		status = MI_ERR;
	}

	return status;
}

/*
* Function Name: MFRC522_Anticoll
* Description: Anti-collision detection, reading selected card serial number card
* Input parameters: serNum - returns 4 bytes card serial number, the first 5 bytes for the checksum byte
* Return value: the successful return MI_OK
*/

uint8_t MFRC522_Anticoll(uint8_t *serNum)
{
	uint8_t status;
	uint8_t i;
	uint8_t serNumCheck=0;
	uint16_t unLen;

	//ClearBitMask(Status2Reg, 0x08);	//TempSensclear
	//ClearBitMask(CollReg,0x80);		//ValuesAfterColl
	Write_MFRC522(BitFramingReg, 0x00);		//TxLastBists = BitFramingReg[2..0]

	serNum[0] = PICC_ANTICOLL;
	serNum[1] = 0x20;
	status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

	if (status == MI_OK)
	{
		//Check card serial number
		for (i = 0; i < 4; i++)
		{
			serNumCheck ^= serNum[i];
		}

		if (serNumCheck != serNum[i])
		{
			status = MI_ERR;
		}
	}

	//SetBitMask(CollReg, 0x80);    //ValuesAfterColl=1

	return status;
}

/*
* Function Name: CalulateCRC
* Description: CRC calculation with MF522
* Input parameters: pIndata - To read the CRC data, len - the data length, pOutData - CRC calculation results
* Return value: None
*/

void CalulateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData)
{
	uint8_t i, n;

	ClearBitMask(DivIrqReg, 0x04);	//CRCIrq = 0
	SetBitMask(FIFOLevelReg, 0x80);	//Clear the FIFO pointer
	//Write_MFRC522(CommandReg, PCD_IDLE);

	//Writing data to the FIFO
	for (i = 0; i < len; i++)
	{
		Write_MFRC522(FIFODataReg, *(pIndata+i));
	}

	Write_MFRC522(CommandReg, PCD_CALCCRC);

	//Wait CRC calculation is complete
	i = 0xFF;

	do 
	{
		n = Read_MFRC522(DivIrqReg);
		i--;
	}

	while ((i!=0) && !(n&0x04));	//CRCIrq = 1

	//Read CRC calculation result
	pOutData[0] = Read_MFRC522(CRCResultRegL);
	pOutData[1] = Read_MFRC522(CRCResultRegM);
}

/*
 * Function Name: MFRC522_SelectTag
 * Description: election card, read the card memory capacity
 * Input parameters: serNum - Incoming card serial number
 * Return value: the successful return of card capacity
 */
uint8_t MFRC522_SelectTag(uint8_t *serNum)
{
		uint8_t i;
	uint8_t status;
	uint8_t size;
		uint16_t recvBits;
		uint8_t buffer[9]; 

	//ClearBitMask(Status2Reg, 0x08);     //MFCrypto1On=0

	buffer[0] = PICC_SElECTTAG;
	buffer[1] = 0x70;

	for (i = 0; i < 5; i++)
	{
		buffer[i+2] = *(serNum+i);
	}

	CalulateCRC(buffer, 7, &buffer[7]);	//??

	status = MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 9, buffer, &recvBits);
		
	if ((status == MI_OK) && (recvBits == 0x18))
	{
		size = buffer[0];
	}
	else
	{
		size = 0;
	}

	return size;
}


/*
 * Function Name: MFRC522_Auth
 * Description: Verify card password
 * Input parameters: authMode - Password Authentication Mode
								 0x60 = A key authentication
								 0x61 = Authentication Key B
						 BlockAddr--Block address
						 Sectorkey--Sector password
						 serNum--Card serial number, 4-byte
 * Return value: the successful return MI_OK
 */
uint8_t MFRC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t *Sectorkey, uint8_t *serNum)
{
	uint8_t status;
	uint16_t recvBits;
	uint8_t i;
	uint8_t buff[12]; 

	//Verify the command block address + sector + password + card serial number
	buff[0] = authMode;
	buff[1] = BlockAddr;

	for (i = 0; i < 6; i++)
	{
		buff[i+2] = *(Sectorkey+i);
	}

	for (i = 0; i < 4; i++)
	{
		buff[i+8] = *(serNum+i);
	}

	status = MFRC522_ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);

	if ((status != MI_OK) || (!(Read_MFRC522(Status2Reg) & 0x08)))
	{
		status = MI_ERR;
	}
		
	return status;
}

/*
* Function Name: MFRC522_Read
* Description: Read block data
* Input parameters: blockAddr - block address; recvData - read block data
* Return value: the successful return MI_OK
*/

uint8_t MFRC522_Read(uint8_t blockAddr, uint8_t *recvData)
{
	uint8_t status;
	uint16_t unLen;

	recvData[0] = PICC_READ;
	recvData[1] = blockAddr;
	CalulateCRC(recvData,2, &recvData[2]);
	status = MFRC522_ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

	if ((status != MI_OK) || (unLen != 0x90))
	{
		status = MI_ERR;
	}

	return status;
}

/*
 * Function Name: MFRC522_Write
 * Description: Write block data
 * Input parameters: blockAddr - block address; writeData - to 16-byte data block write
 * Return value: the successful return MI_OK
 */

uint8_t MFRC522_Write(uint8_t blockAddr, uint8_t *writeData)
{
	uint8_t status;
	uint16_t recvBits;
	uint8_t i;
	uint8_t buff[18]; 

	buff[0] = PICC_WRITE;
	buff[1] = blockAddr;
	CalulateCRC(buff, 2, &buff[2]);
	status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);

	if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))
	{
		status = MI_ERR;
	}

	if (status == MI_OK)
	{
		for (i=0; i<16; i++)	//Data to the FIFO write 16Byte
		{
			buff[i] = *(writeData+i);
		}

		CalulateCRC(buff, 16, &buff[16]);
		status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);
		
		if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))
		{
			status = MI_ERR;
		}
	}

	return status;
}

/*
* Function Name: MFRC522_Halt
* Description: Command card into hibernation
* Input: None
* Return value: None
*/

void MFRC522_Halt(void)
{
	uint8_t status;
	uint16_t unLen;
	uint8_t buff[4]; 

	buff[0] = PICC_HALT;
	buff[1] = 0;
	CalulateCRC(buff, 2, &buff[2]);

	status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff,&unLen);
}

String convertHEX (int symbol) {
	String stringVar = String(symbol, HEX);

	if (stringVar.length() < 2)
	{
		stringVar = "0" + stringVar;
	}

	stringVar.replace("a", "A");
	stringVar.replace("b", "B");
	stringVar.replace("c", "C");
	stringVar.replace("d", "D");
	stringVar.replace("e", "E");
	stringVar.replace("f", "F");

	return stringVar; 
}
