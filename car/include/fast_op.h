#ifndef FAST_OP_H_
#define FAST_OP_H_
	#define bitSet(b, p)				b |= (1 << p)
	#define bitGet(b, p)				(b & (1 << p))
	#define bitUnSet(b, p)				b &= ~(1 << p)

	/*EEPROM Начало*/
	#define EEPROMWriteData(iD, ba)	WriteDataEEPROM_Sized(&iD, ba, sizeof(iD));
	#define EEPROMReadData(iD, ba)	ReadDataEEPROM_Sized(&iD, ba, sizeof(iD));

	void WriteDataEEPROM_Sized(const void *iData, uint8_t byte_adress, uint8_t DataSize)
	{
		uint8_t		*byte_Data	= (uint8_t *) iData,
					block		= 0;

		for(block	= 0; block < DataSize; block++)
		{
			eeprom_write_byte((uint8_t*)(block + byte_adress), *byte_Data);
			byte_Data++;
		}
	}

	void ReadDataEEPROM_Sized(const void *iData, uint8_t byte_adress, uint8_t DataSize)
	{
		uint8_t		*byte_Data	= (uint8_t *) iData,
					block		= 0;

		for(block	= 0; block < DataSize; block++)
		{
			*byte_Data = eeprom_read_byte((uint8_t*)(block + byte_adress));
			byte_Data++;
		}
	}
	/*EEPROM Конец*/

	void strcp(const char* source, uint8_t *receiver)
	{
		uint8_t i = 0;

		for (;;)
		{
			receiver[i] = source[i];

			if (receiver[i] == '\0')
			{
				break;
			}

			i++;
		}
	}

	void strcp(const char* source, uint8_t *receiver, uint8_t length)
	{
		uint8_t i = 0;

		for (i; i < length; ++i)
		{
			receiver[i] = source[i];
		}
	}

	void strclr(uint8_t* source, uint8_t length)
	{
		uint8_t i = 0;

		for (i; i < length; ++i)
		{
			source[i] = '\0';
		}
	}

	uint8_t strcmp(const uint8_t* source, const uint8_t *source2, uint8_t length)
	{
		uint8_t i = 0;

		for (i; i < length; ++i)
		{
			if (source2[i] != source[i])
			{
				return 0;
			}
		}

		return 1;
	}
#endif