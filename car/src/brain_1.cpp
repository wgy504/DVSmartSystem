#include <Arduino.h>				// Для работы Serial
#include <Wire.h>					// Подключаем библиотеку Wire
#include <TimeLib.h>				// Подключаем библиотеку TimeLib
#include <DS1307RTC.h>				// Подключаем библиотеку DS1307RTC
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "config.h"

#define VERSION_BRAIN_1				1

// Delays
#define DELAY_SAVE	300000

#define bitSet(b, p)				b |= (1 << p)
#define bitGet(b, p)				(b & (1 << p))
#define bitUnSet(b, p)				b &= ~(1 << p)

//Quick operations
#define setPartFirst(b, v)			b = v;\
									b = (b << 4)
#define setPartSecond(b, v)			b += v
#define getPartFirst(b)				((b >> 4) & 0x0f)
#define getPartSecond(b)			(b & 0x0f)

#define SET_BRIGHT_L_FOG(v)			OCR1A = v
#define GET_BRIGHT_L_FOG()			OCR1A
#define SET_BRIGHT_R_FOG(v)			OCR0A = v
#define GET_BRIGHT_R_FOG()			OCR0A
#define SET_BRIGHT_SALOON(v)		OCR0B = v
#define GET_BRIGHT_SALOON()			OCR0B

#define SET_MIRRORS_HEATING_ON()	bitSet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING);\
									time_mirrors_heating = millis();\
									state_mirrors_heating = 1

#define SET_MIRRORS_HEATING_OFF()	bitUnSet(bytes_shift_register[0], BIT_F_MIRROR_HEATING);\
									bitUnSet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING);\
									time_mirrors_heating = millis();\
									state_mirrors_heating = 0

// first byte
#define BIT_F_LIGHTS_1LVL			0	// Подсветка блока управления температурой
#define BIT_F_LIGHTS_2LVL			1	// Подсветка торпеды
// #define BIT_F_LIGHTS_3LVL			2	// Подсветка ног
#define BIT_F_LIGHTS_4LVL			3	// Посветка зеркал
#define BIT_F_FOG_LIGHTS			4	// Противотуманки
#define BIT_F_LOW_LIGHTS			5	// Дневные ходовые огни
#define BIT_F_HIGHLIGHTS			6	// Ближний свет
#define BIT_F_MIRROR_HEATING		7	// Подогрев зеркал

#define BIT_S_FAN_LEVEL_1			0
#define BIT_S_FAN_LEVEL_2			1
#define BIT_S_FAN_LEVEL_3			2
#define BIT_S_FAN_LEVEL_4			3
#define BIT_S_GLASS_HEATING			4	// Подогрев заднего стекла
#define BIT_S_USB					7	// Разъём USB

#define BIT_T_MUSIC					0	// Магнитола
#define BIT_T_REGISTRATOR			1	// Марина
#define BIT_T_LIGHTS_3LVL			5	// Подогрев задницы пассажира уроень 1
// #define BIT_T_ASS_PASS_1			5	// Подогрев задницы пассажира уроень 1
#define BIT_T_ASS_PASS_2			6	// Подогрев задницы пассажира уроень 2
#define BIT_T_ASS_DRV_1				3	// Подогрев задницы 
#define BIT_T_ASS_DRV_2				4	// Подогрев задницы водителя

// Shift register pins
#define PIN_D_DATA					4
#define PIN_D_CLOCK					7
#define PIN_B_LATCH					0

// PINS
// FOG
#define PIN_D_LIGHT					5
#define PIN_D_FOG_R					6
#define PIN_B_FOG_L					1
// FERNS
#define PIN_B_FERN_L				5	
#define PIN_B_FERN_R				4	

// Systems bits
#define SYS_BIT_AUTO_HEADLIGHT		0
#define SYS_BIT_AUTO_FAN			1
#define SYS_BIT_MIRRORS_HEATING		2
#define SYS_BIT_FOG					3
#define SYS_BIT_AUTO_DAYLIGHT		4
#define SYS_BIT_FERNS_FOG			5

// Temp bits
#define TMP_BIT_FOG_ON				0
#define TMP_BIT_SYSTEM_SAVE			1
#define TMP_BIT_DRIVER_INSIDE		2
#define TMP_BIT_ENGINE_WORK			3
#define TMP_BIT_SALOON_LIGHT		4
#define TMP_BIT_SLEEP				5
#define TMP_BIT_HEADLIGHT			6
#define TMP_BIT_FERN_L				7
#define TMP_BIT_FERN_R				8
#define TMP_BIT_FERN_ON				9
#define TMP_BIT_DIBILMODE			10

struct structSystem
{
	uint8_t sys_byte,
			sys_headlight_time_on,
			sys_headlight_time_off,
			sys_bright_saloon,
			sys_mirrors_time_off,
			sys_mirrors_time_on,
			sys_fan_speed;
};

volatile uint8_t value = 0,
		i = 0,
		count = 0,
		count_dibil = 0,
		protocol_array[5] = {},
		state_mirrors_heating = 0;

uint8_t bytes_shift_register[3] = {0, 0, 0};

volatile uint16_t byte_tmp = 0;

volatile uint64_t lasttime = 0,
		time_save = 0,
		time_saloon_light = 0,
		time_highlight = 0,
		time_fog = 0,
		time_fern = 0,
		time_mirrors_heating = 0,
		time_data = 0;

tmElements_t tm;
volatile structSystem system_data = {};

#define GET_STATUS_FOG 	bitGet(byte_tmp, TMP_BIT_FOG_ON)

#define SET_FOG_ON		{\
							bitSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);\
							bitSet(byte_tmp, TMP_BIT_FOG_ON);\
							time_fog = millis();\
						}

#define SET_FOG_OFF		{\
							bitUnSet(byte_tmp, TMP_BIT_FOG_ON);\
							time_fog = millis();\
						}

#define GET_STATUS_SALOON_LIGHT bitGet(byte_tmp, TMP_BIT_SALOON_LIGHT)

#define SET_SALOON_LIGHT_ON {\
								bitSet(byte_tmp, TMP_BIT_SALOON_LIGHT);\
								time_saloon_light = millis();\
							}

#define SET_SALOON_LIGHT_OFF {\
								bitUnSet(byte_tmp, TMP_BIT_SALOON_LIGHT);\
								time_saloon_light = millis();\
							}

SIGNAL(TIMER0_COMPA_vect)
{

	// Обновление сдвигового регистра
	if ((!bitGet(byte_tmp, TMP_BIT_SLEEP))  && ((millis() - lasttime) >= 50))
	{
		systemUpdate(bytes_shift_register);
		lasttime = millis();
	}

	// Таймер работы обогревва зеркал
	if ((bitGet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING)) && (!bitGet(byte_tmp, TMP_BIT_SLEEP)) && (bitGet(byte_tmp, TMP_BIT_ENGINE_WORK)))
	{
		// Включено
		if (state_mirrors_heating == 1)
		{
			if ((millis() - time_mirrors_heating) >= (system_data.sys_mirrors_time_on * 60000))
			{
				time_mirrors_heating = millis();
				state_mirrors_heating = 2;
			}
			else
			{
				bitSet(bytes_shift_register[0], BIT_F_MIRROR_HEATING);
			}
		}

		// Отключено
		if (state_mirrors_heating == 2)
		{
			if ((millis() - time_mirrors_heating) >= (system_data.sys_mirrors_time_off * 60000))
			{
				time_mirrors_heating = millis();
				state_mirrors_heating = 1;
			}
			else
			{
				bitUnSet(bytes_shift_register[0], BIT_F_MIRROR_HEATING);
			}
		}
	}

	// Таймер сохранения настроек
	if (bitGet(byte_tmp, TMP_BIT_SYSTEM_SAVE))
	{
		if ((millis() - time_save) >= DELAY_SAVE)
		{
			bitUnSet(byte_tmp, TMP_BIT_SYSTEM_SAVE);
			// saveSettings();
		}
	}

	if (!bitGet(byte_tmp, TMP_BIT_DIBILMODE))
	{
		// Обслуживание противотуманок
		if (bitGet(byte_tmp, TMP_BIT_FOG_ON))
		{
			bitUnSet(byte_tmp, TMP_BIT_FERN_ON);
			bitUnSet(byte_tmp, TMP_BIT_FERN_L);
			bitUnSet(byte_tmp, TMP_BIT_FERN_R);

			if((GET_BRIGHT_L_FOG() != 255) && ((millis() - time_fog) >= 10))
			{
				SET_BRIGHT_L_FOG(GET_BRIGHT_L_FOG() + 1);
				SET_BRIGHT_R_FOG(GET_BRIGHT_R_FOG() + 1);
				time_fog = millis();
			}
		}
		else
		{
			if ((!bitGet(byte_tmp, TMP_BIT_FERN_ON)) && (!bitGet(byte_tmp, TMP_BIT_FERN_L) && (!bitGet(byte_tmp, TMP_BIT_FERN_R))))
			{	
				if ((GET_BRIGHT_L_FOG() != 0) && ((millis() - time_fog) >= 10))
					{
						SET_BRIGHT_L_FOG(GET_BRIGHT_L_FOG() - 1);
						SET_BRIGHT_R_FOG(GET_BRIGHT_R_FOG() - 1);
						time_fog = millis();
					}
		
					if ((bitGet(bytes_shift_register[0], BIT_F_FOG_LIGHTS)) && (GET_BRIGHT_L_FOG() == 0))
					{
						bitUnSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);
					}
			}

			// Выключение противотуманки после выключения поворотника
			if ((bitGet(byte_tmp, TMP_BIT_FERN_ON)) && (!bitGet(byte_tmp, TMP_BIT_FERN_L)) && (!bitGet(byte_tmp, TMP_BIT_FERN_R)) && (GET_BRIGHT_L_FOG() == 0) && (GET_BRIGHT_R_FOG() == 0))
			{
				bitUnSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);
				bitUnSet(byte_tmp, TMP_BIT_FERN_ON);
			}

			if ((bitGet(byte_tmp, TMP_BIT_FERN_ON)) && (!bitGet(byte_tmp, TMP_BIT_FERN_L)) && (GET_BRIGHT_L_FOG() != 0))
			{
				if ((millis() - time_fog) >= 5)
				{
					time_fog = millis();
					SET_BRIGHT_L_FOG(GET_BRIGHT_L_FOG() - 1);
				}
			}

			if ((bitGet(byte_tmp, TMP_BIT_FERN_ON)) && (!bitGet(byte_tmp, TMP_BIT_FERN_R)) && (GET_BRIGHT_R_FOG() != 0))
			{
				if ((millis() - time_fog) >= 5)
				{
					time_fog = millis();
					SET_BRIGHT_R_FOG(GET_BRIGHT_R_FOG() - 1);
				}
			}
			
			// Включение противотуманки при включения поворотника
			if ((bitGet(byte_tmp, TMP_BIT_FERN_ON)) && (bitGet(byte_tmp, TMP_BIT_FERN_L)) && (GET_BRIGHT_L_FOG() != 255))
			{
				if ((millis() - time_fog) >= 2)
				{
					time_fog = millis();
					SET_BRIGHT_L_FOG(GET_BRIGHT_L_FOG() + 1);
				}
			}

			if ((bitGet(byte_tmp, TMP_BIT_FERN_ON)) && (bitGet(byte_tmp, TMP_BIT_FERN_R)) && (GET_BRIGHT_R_FOG() != 255))
			{
				if ((millis() - time_fog) >= 2)
				{
					time_fog = millis();
					SET_BRIGHT_R_FOG(GET_BRIGHT_R_FOG() + 1);
				}
			}

		}

		// Противотуманки при поворотниках
		if ((bitGet(system_data.sys_byte, SYS_BIT_FERNS_FOG)) && (bitGet(bytes_shift_register[0], BIT_F_HIGHLIGHTS)) && (!bitGet(byte_tmp, GET_STATUS_FOG)))
		{

			if (((!bitGet(PINB, PIN_B_FERN_L)) && (bitGet(PINB, PIN_B_FERN_R))) || ((bitGet(PINB, PIN_B_FERN_L)) && (!bitGet(PINB, PIN_B_FERN_R))))
			{
				time_fern = millis();
				bitSet(byte_tmp, TMP_BIT_FERN_ON);

				if (!bitGet(PINB, PIN_B_FERN_L))
				{
					bitSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);
					bitUnSet(byte_tmp, TMP_BIT_FERN_R);
					bitSet(byte_tmp, TMP_BIT_FERN_L);
					SET_BRIGHT_R_FOG(0);
				}

				if (!bitGet(PINB, PIN_B_FERN_R))
				{
					bitSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);
					bitSet(byte_tmp, TMP_BIT_FERN_R);
					bitUnSet(byte_tmp, TMP_BIT_FERN_L);
					SET_BRIGHT_L_FOG(0);
				}
			}

			if (((bitGet(byte_tmp, TMP_BIT_FERN_L)) || (bitGet(byte_tmp, TMP_BIT_FERN_R))))
			{
				if (((millis() - time_fern) >= 1000) && ((bitGet(PINB, PIN_B_FERN_L)) || (bitGet(PINB, PIN_B_FERN_R))))
				{
					bitUnSet(byte_tmp, TMP_BIT_FERN_L);
					bitUnSet(byte_tmp, TMP_BIT_FERN_R);
				}
			}
		}
	}

	// Обслуживание подсветки салона
	if (bitGet(byte_tmp, TMP_BIT_SALOON_LIGHT))
	{
		if ((GET_BRIGHT_SALOON() != system_data.sys_bright_saloon) && ((millis() - time_saloon_light) >= 20))
		{
			SET_BRIGHT_SALOON(GET_BRIGHT_SALOON() + 1);
			time_saloon_light = millis();
		}
	}
	else
	{
		if ((GET_BRIGHT_SALOON() != 0) && ((millis() - time_saloon_light) >= 20))
		{
			SET_BRIGHT_SALOON(GET_BRIGHT_SALOON() - 1);
			time_saloon_light = millis();
		}
	}

	// Обслуживание систем автоматического назначения
	if (!bitGet(byte_tmp, TMP_BIT_SLEEP))
	{
		systemRefresh();
	}

	if (bitGet(byte_tmp, TMP_BIT_DIBILMODE))
	{
		if ((count_dibil == 0) || (count_dibil == 4) || (count_dibil == 8))
		{
			SET_BRIGHT_R_FOG(255);
			time_fog = millis();
			count_dibil++;
			return;
		}

		if ((count_dibil == 1) ||
			(count_dibil == 3) ||
			(count_dibil == 5) ||
			(count_dibil == 7) ||
			(count_dibil == 9) ||
			(count_dibil == 13) ||
			(count_dibil == 15) ||
			(count_dibil == 17) ||
			(count_dibil == 19) ||
			(count_dibil == 21))
		{
			if ((millis() - time_fog) >= 75) {
				count_dibil++;
				return;
			}
		}

		if ((count_dibil == 2) || (count_dibil == 6) || (count_dibil == 10))
		{
			SET_BRIGHT_R_FOG(0);
			time_fog = millis();
			count_dibil++;
			return;
		}

		if ((count_dibil == 11))
		{
			if ((millis() - time_fog) >= 750) {
				count_dibil++;
				return;
			}
		}

		if ((count_dibil == 12) || (count_dibil == 16) || (count_dibil == 20))
		{
			SET_BRIGHT_L_FOG(255);
			time_fog = millis();
			count_dibil++;
			return;
		}

		if ((count_dibil == 14) || (count_dibil == 18) || (count_dibil == 22))
		{
			SET_BRIGHT_L_FOG(0);
			time_fog = millis();
			count_dibil++;
			return;
		}

		if ((count_dibil == 23))
		{
			count_dibil = 0;
			return;
		}
	}
}

void watch(void)
{
	if ((millis() - time_highlight) >= 5000)
	{
		RTC.read(tm);

		if ((bitGet(byte_tmp, TMP_BIT_HEADLIGHT)) && (bitGet(system_data.sys_byte, SYS_BIT_AUTO_HEADLIGHT)))
		{
			//  Условие на включение ближнего света
			if ((!bitGet(bytes_shift_register[0], BIT_F_HIGHLIGHTS)) &&
				(((system_data.sys_headlight_time_on <= tm.Hour) && (system_data.sys_headlight_time_off < tm.Hour)) ||
				((system_data.sys_headlight_time_on > tm.Hour) && (system_data.sys_headlight_time_off > tm.Hour))))
			{
				bitSet(bytes_shift_register[0], BIT_F_HIGHLIGHTS);
			}

			//  Условие на выключение ближнего света
			if (bitGet(bytes_shift_register[0], BIT_F_HIGHLIGHTS) &&
				((system_data.sys_headlight_time_off <= tm.Hour) && (system_data.sys_headlight_time_on > tm.Hour)))
			{
				bitUnSet(bytes_shift_register[0], BIT_F_HIGHLIGHTS);
			}
		}

		time_highlight = millis();
	}
}

void WriteDataEEPROM(void *iData, uint8_t byte_adress, uint8_t DataSize) {
	uint8_t		*byte_Data	= (uint8_t *) iData,
				block		= 0;

	for(block	= 0; block < DataSize; block++) {
		eeprom_write_byte((uint8_t*)(block + byte_adress), *byte_Data);
		byte_Data++;
	}
}

void ReadDataEEPROM(void *iData, uint8_t byte_adress, uint8_t DataSize) {
	uint8_t		*byte_Data	= (uint8_t *) iData,
				block		= 0;

	for(block	= 0; block < DataSize; block++) {
		*byte_Data = eeprom_read_byte((uint8_t*)(block + byte_adress));
		byte_Data++;
	}
}

void systemSave(void)
{
	bitSet(byte_tmp, TMP_BIT_SYSTEM_SAVE);
	time_save = millis();
}	

void writeCommand(void)
{
	Serial.write(protocol_array[4]);

	_delay_ms(15);

	for (uint8_t i = 0; i < protocol_array[4]; ++i)
	{
		Serial.write(protocol_array[i]);
		_delay_ms(10);
	}

	protocol_array[0] = 0;
	protocol_array[1] = 0;
	protocol_array[2] = 0;
	protocol_array[3] = 0;
	protocol_array[4] = 0;
}

void dataWait(void)
{
	while (!Serial.available())
		watch();

	count = Serial.read();

	for (uint8_t i = 0; i < count; ++i)
	{
		time_data = millis();

		while (!Serial.available())
		{
			if ((millis() - time_data) >= 1000)
			{
				dataClear();
				return;
			}
		}

		protocol_array[i] = Serial.read();
	}

	for (uint8_t i = count; i < 5; ++i)
		protocol_array[i] = 0;
}

uint8_t dataRead(uint8_t index)
{
	uint8_t cache = protocol_array[index];
	protocol_array[index] = 0;
	return cache;
}

uint8_t dataWrite(uint8_t elem0, uint8_t elem1, uint8_t elem2, uint8_t elem3, uint8_t count)
{
	protocol_array[0] = elem0;
	protocol_array[1] = elem1;
	protocol_array[2] = elem2;
	protocol_array[3] = elem3;
	protocol_array[4] = count;
	writeCommand();
}

void dataClear(void)
{
	protocol_array[0] = 0;
	protocol_array[1] = 0;
	protocol_array[2] = 0;
	protocol_array[3] = 0;
}

void systemRefresh(void)
{
	// Если водитель в машине
	if (bitGet(byte_tmp, TMP_BIT_DRIVER_INSIDE))
	{
		// Включение  магнитолы
		bitSet(bytes_shift_register[2], BIT_T_MUSIC);

		// Включение  USB
		bitSet(bytes_shift_register[1], BIT_S_USB);
	}
	else
	{
		// Выключение  магнитолы
		bitUnSet(bytes_shift_register[2], BIT_T_MUSIC);

		// Выключение  USB
		bitUnSet(bytes_shift_register[1], BIT_S_USB);
	}

	if (bitGet(byte_tmp, TMP_BIT_ENGINE_WORK))
	{
		// Включение видеорегистратора
		if (!bitGet(bytes_shift_register[2], BIT_T_REGISTRATOR))
		{
			bitSet(bytes_shift_register[2], BIT_T_REGISTRATOR);
		}

		// Если водитель в машине - включить его настройки
		if (bitGet(byte_tmp, TMP_BIT_DRIVER_INSIDE))
		{
			// Управление вентилятором
			if (system_data.sys_fan_speed == 1)
				bitSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_1);
			else
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_1);

			if (system_data.sys_fan_speed == 2)
				bitSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_2);
			else
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_2);

			if (system_data.sys_fan_speed == 3)
				bitSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_3);
			else
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_3);

			if (system_data.sys_fan_speed == 4)
				bitSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_4);
			else
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_4);

			// Противотуманки как ДХО
			if (bitGet(system_data.sys_byte, SYS_BIT_AUTO_DAYLIGHT))
			{
				if ((!bitGet(bytes_shift_register[0], BIT_F_FOG_LIGHTS)) &&
					(!bitGet(bytes_shift_register[0], BIT_F_HIGHLIGHTS)))
				{
					SET_FOG_ON
				}

				if ((bitGet(bytes_shift_register[0], BIT_F_FOG_LIGHTS)) &&
					(!bitGet(system_data.sys_byte, SYS_BIT_FOG)) &&
					(GET_STATUS_FOG) &&
					(bitGet(bytes_shift_register[0], BIT_F_HIGHLIGHTS)))
				{
					SET_FOG_OFF
				}
			}

			// Управление противотуманками
			if ((!GET_STATUS_FOG) && (bitGet(system_data.sys_byte, SYS_BIT_FOG)))
			{
				SET_FOG_ON
			}

			if (!GET_STATUS_SALOON_LIGHT)
			{	
				SET_SALOON_LIGHT_ON
			}

			if (!bitGet(byte_tmp, TMP_BIT_HEADLIGHT))
			{
				bitSet(byte_tmp, TMP_BIT_HEADLIGHT);
			}
		}
		else
		{
			// Если водитель не в машине - включить 2-ю скорость
			if (bitGet(system_data.sys_byte, SYS_BIT_AUTO_FAN))
			{
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_1);
				bitSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_2);
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_3);
				bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_4);
			}
		}
	}
	else
	{
		// Отключение обогрева зеркал
		if (bitGet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING))
		{
			bitUnSet(bytes_shift_register[0], BIT_F_MIRROR_HEATING);
		}

		// Отключение вентилятора
		bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_1);
		bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_2);
		bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_3);
		bitUnSet(bytes_shift_register[1], BIT_S_FAN_LEVEL_4);
		// Отключение обогрева зеркал заднего вида
		bitUnSet(bytes_shift_register[0], BIT_F_MIRROR_HEATING);
		// Отключение противотуманок
		if (GET_STATUS_FOG)
		{
			SET_FOG_OFF
		}
		// Отключение подсветки салона
		if (GET_STATUS_SALOON_LIGHT)
		{
				SET_SALOON_LIGHT_OFF
		}
		// Выключение видеорегистратора
		bitUnSet(bytes_shift_register[2], BIT_T_REGISTRATOR);

		// Отключение автоматически включённого ближнего света
		bitUnSet(byte_tmp, TMP_BIT_HEADLIGHT);
		// Отключение ближнего света
		bitUnSet(bytes_shift_register[0], BIT_F_HIGHLIGHTS);
	}
}

void systemUpdate(uint8_t *data)
{
	for (uint8_t j = 0; j < 3; ++j)
	{
		for (uint8_t i = 0; i < 8; ++i)
		{
			if (data[j] & (1 << i))
				PORTD |= (1 << PIN_D_DATA);
			else
				PORTD &= ~(1 << PIN_D_DATA);

			// Запись данных
			PORTD |= (1 << PIN_D_CLOCK);
			PORTD &= ~(1 << PIN_D_CLOCK);
		}
	}

	// Вывод данных на пины
	PORTB |= (1 << PIN_B_LATCH);
	PORTB &= ~(1 << PIN_B_LATCH);
}

void setup(void)
{
	// Настройка пинов для сдвиговых регистров
	bitSet(DDRD, PIN_D_DATA);
	bitSet(DDRD, PIN_D_CLOCK);
	bitSet(DDRB, PIN_B_LATCH);

	// Пины противотуманок
	bitSet(DDRD, PIN_D_LIGHT);
	bitSet(DDRD, PIN_D_FOG_R);
	bitSet(DDRB, PIN_B_FOG_L);

	// Поворотники
	bitUnSet(DDRB, PIN_B_FERN_R);
	bitUnSet(DDRB, PIN_B_FERN_L);

	Serial.begin(9600);

	systemUpdate(bytes_shift_register);

	// ШИМ для подсветки панели
	OCR0B = 0;
	bitSet(TCCR0A, COM0B1);

	// Прерывания по таймеру
	OCR0A = 0xAF;
	TIMSK0 |= _BV(OCIE0A);

	// ШИМ правой противотуманки
	OCR0A = 0;
	bitSet(TCCR0A, COM0A1);

	// ШИМ для левой противотуманки
	bitSet(TCCR1A, COM1A1);
	OCR1A = 0;

	// Загрузка данных из EEPROM
	// ReadDataEEPROM(&system_data, 0, sizeof(system_data));

	// Изначально водитель должен быть в машине (кто же ещё прошьёт мозги!?)
	bitSet(byte_tmp, TMP_BIT_DRIVER_INSIDE);

	// Временные стандартные настройки
	bitSet(system_data.sys_byte, SYS_BIT_AUTO_HEADLIGHT);
	bitSet(system_data.sys_byte, SYS_BIT_AUTO_FAN);
	bitSet(system_data.sys_byte, SYS_BIT_AUTO_DAYLIGHT);
	bitSet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING);
	bitSet(system_data.sys_byte, SYS_BIT_FERNS_FOG);
	system_data.sys_mirrors_time_on = 10;
	system_data.sys_mirrors_time_off = 20;
	system_data.sys_headlight_time_on = 15;
	system_data.sys_headlight_time_off = 10;
	system_data.sys_bright_saloon = 100;
	system_data.sys_fan_speed = 2;
}

void loop(void)
{	
	dataWait();

	switch (dataRead(0))
	{
		// Настройка времени
		case COM_SET_TIME:
		{
			tm.Hour = dataRead(1);
			tm.Minute = dataRead(2);
			RTC.write(tm);
		}
		break;

		// Настройка даты
		case COM_SET_DATE:
		{
			tm.Day = dataRead(1);
			tm.Month = dataRead(2);
			// tm.Year = dataRead(3);
			RTC.write(tm);
		}
		break;

		// Отправка времени
		case COM_GET_TIME:
		{
			RTC.read(tm);
			dataWrite(COM_GET_TIME, tm.Hour, tm.Minute, 0, 3);	
		}
		break;

		// Отправка информации о вентиляторе
		case COM_GET_FAN:
		{
			dataWrite(COM_GET_FAN, system_data.sys_fan_speed, 0, 0, 2);
		}
		break;

		// Настройка вентилятора
		case COM_SET_FAN:
		{
			system_data.sys_fan_speed = dataRead(1);
			systemSave();
		}
		break;

		// Отправка информации о подсветке салона
		case COM_GET_LIGHT_SALOON:
		{
			value = 0;

			if (bytes_shift_register[0] & (1 << BIT_F_LIGHTS_1LVL))
				bitSet(value, 0);

			if (bytes_shift_register[0] & (1 << BIT_F_LIGHTS_2LVL))
				bitSet(value, 1);

			if (bytes_shift_register[2] & (1 << BIT_T_LIGHTS_3LVL))
				bitSet(value, 2);

			if (bytes_shift_register[0] & (1 << BIT_F_LOW_LIGHTS))
				bitSet(value, 3);

			dataWrite(COM_GET_LIGHT_SALOON, value, 0, 0, 2);
			systemSave();
		}
		break;

		// Настройка подсветки салона
		case COM_SET_LIGHT_SALOON:
		{
			value = dataRead(1);

			if (bitGet(value, 0))
				bytes_shift_register[0] |= (1 << BIT_F_LIGHTS_1LVL);
			else
				bytes_shift_register[0] &= ~(1 << BIT_F_LIGHTS_1LVL);

			if (bitGet(value, 1))
				bytes_shift_register[0] |= (1 << BIT_F_LIGHTS_2LVL);
			else
				bytes_shift_register[0] &= ~(1 << BIT_F_LIGHTS_2LVL);

			if (bitGet(value, 2))
				bytes_shift_register[2] |= (1 << BIT_T_LIGHTS_3LVL);
			else
				bytes_shift_register[2] &= ~(1 << BIT_T_LIGHTS_3LVL);

			if (bitGet(value, 3))
				bytes_shift_register[0] |= (1 << BIT_F_LOW_LIGHTS);
			else
				bytes_shift_register[0] &= ~(1 << BIT_F_LOW_LIGHTS);
		}
		break;

		// Отправка информации о яркости подсветки
		case COM_GET_LIGHT_BRIGHT:
		{
			dataWrite(COM_GET_LIGHT_BRIGHT, system_data.sys_bright_saloon, 0, 0, 2);
		}
		break;

		// Настройка яркости подсветки
		case COM_SET_LIGHT_BRIGHT:
		{
			system_data.sys_bright_saloon = dataRead(1);
			OCR0B = system_data.sys_bright_saloon;
			systemSave();
		}
		break;

		// Получено сообщение о включении двигателя
		case COM_SET_ENGINE_WORK:
		{
			bitSet(byte_tmp, TMP_BIT_ENGINE_WORK);
			if (bitGet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING))
			{
				SET_MIRRORS_HEATING_ON();
			}
		}
		break;

		// Получено сообщение о выключении двигателя
		case COM_SET_ENGINE_NOT_WORK:
		{
			bitUnSet(byte_tmp, TMP_BIT_ENGINE_WORK);
		}
		break;

		// Перево второго мозга в нормальный режим
		case COM_SET_NORMAL_MODE:
		{
			bitUnSet(byte_tmp, TMP_BIT_SLEEP);
		}
		break;

		// Перево второго мозга в экономный режим
		case COM_SET_LOWPOWER_MODE:
		{
			cli();
			bitSet(byte_tmp, TMP_BIT_SLEEP);
			bitUnSet(byte_tmp, TMP_BIT_DRIVER_INSIDE);
			bitUnSet(byte_tmp, TMP_BIT_ENGINE_WORK);
			systemRefresh();
			uint8_t eco_array[3] = {0, 0, 0};
			systemUpdate(eco_array);
			sei();
		}
		break;

		// Отправка информации об автоматическом включении ближнего света
		case COM_GET_AUTO_HEADLIGHT: {
			dataWrite(COM_GET_AUTO_HEADLIGHT, ((system_data.sys_byte & (1 << SYS_BIT_AUTO_HEADLIGHT)) ? 1 : 0), 0, 0, 2);
		}
		break;

		// Установка автоматического света
		case COM_SET_AUTO_HEADLIGHT:
		{
			if (dataRead(1) == 1) {
				bitSet(system_data.sys_byte, SYS_BIT_AUTO_HEADLIGHT);
			} else {
				bitUnSet(system_data.sys_byte, SYS_BIT_AUTO_HEADLIGHT);
				bitUnSet(bytes_shift_register[0], BIT_F_HIGHLIGHTS);
			}

			systemSave();
		}
		break;

		// Установка времени включения ближнего света
		case COM_SET_HEADLIGHT_TIME_ON:
		{
			system_data.sys_headlight_time_on = dataRead(1);
			systemSave();
		}
		break;

		// Установка времени выключения ближнего света
		case COM_SET_HEADLIGHT_TIME_OFF:
		{
			system_data.sys_headlight_time_off = dataRead(1);
			systemSave();
		}
		break;

		// Отправка версии второго мозга
		case COM_GET_VERSION:
		{
			dataWrite(COM_GET_VERSION, VERSION_BRAIN_1, 0, 0, 2);
		}
		break;

		// Отправка информации о подогреве зеркал заднего вида
		case COM_GET_MIRROR_HEATING: {
			dataWrite(COM_GET_MIRROR_HEATING, (bitGet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING) ? 1 : 0), 0, 0, 2);
			systemSave();
		}
		break;

		// Установка подогрева зеркал заднего вида
		case COM_SET_MIRROR_HEATING:
		{
			if (dataRead(1) == 1)
			{

				SET_MIRRORS_HEATING_ON();
				// bitSet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING);
			}
			else
			{
				SET_MIRRORS_HEATING_OFF();
			}
				// bitUnSet(system_data.sys_byte, SYS_BIT_MIRRORS_HEATING);

			systemSave();
		}
		break;

		// Тайм-аут подогреваа зеркал заднего вида 
		case COM_SET_AUTO_MIRROR_TIME:
		{
			system_data.sys_mirrors_time_on = dataRead(1);
			system_data.sys_mirrors_time_off = dataRead(2);
			systemSave();
		}
		break;

		// Отправка информации о подогреве заднего окна
		case COM_GET_WINDOW_HEATING:
		{
			dataWrite(COM_GET_WINDOW_HEATING, (bitGet(bytes_shift_register[1], BIT_S_GLASS_HEATING) ? 1 : 0), 0, 0, 2);
		}
		break;

		// Установка подогрева заднего окна
		case COM_SET_WINDOW_HEATING:
		{
			if (dataRead(1) == 1)
				bitSet(bytes_shift_register[1], BIT_S_GLASS_HEATING);
			else
				bitUnSet(bytes_shift_register[1], BIT_S_GLASS_HEATING);
		}
		break;

		// Отправка информации о противотуманках
		case COM_GET_FOG:
		{
			dataWrite(COM_GET_FOG, (bitGet(system_data.sys_byte, SYS_BIT_FOG) ? 1 : 0), 0, 0, 2);
		}
		break;

		// Отправка информации о противотуманках
		case COM_SET_FOG:
		{
			if (dataRead(1) == 1)
				bitSet(system_data.sys_byte, SYS_BIT_FOG);
			else
			{
				bitUnSet(system_data.sys_byte, SYS_BIT_FOG);
				SET_FOG_OFF
			}

			systemSave();
		}
		break;

		// Установка информации о наличии водителя в машине
		case COM_SET_DRIVER_INSIDE:
		{
			bitSet(byte_tmp, TMP_BIT_DRIVER_INSIDE);
		}
		break;

		// Отправка информации об автоматическом управлении вентилятором
		case COM_GET_FAN_AUTO:
		{
			dataWrite(COM_GET_FOG, (bitGet(system_data.sys_byte, SYS_BIT_AUTO_FAN) ? 1 : 0), 0, 0, 2);
		}
		break;

		// Установка автоматического управления вентилятора
		case COM_SET_FAN_AUTO:
		{
			if (dataRead(1) == 1)
				bitSet(system_data.sys_byte, SYS_BIT_AUTO_FAN);
			else
				bitUnSet(system_data.sys_byte, SYS_BIT_AUTO_FAN);

			systemSave();
		}
		break;

		// Отпрвка информация о противотуманках как ДХО
		case COM_GET_AUTO_DAYLIGHT:
		{
			dataWrite(COM_GET_AUTO_DAYLIGHT, (bitGet(system_data.sys_byte, SYS_BIT_AUTO_DAYLIGHT) ? 1 : 0), 0, 0, 2);
		}
		break;

		// Установка противотуманок как ДХО
		case COM_SET_AUTO_DAYLIGHT:
		{
			if (dataRead(1) == 1)
			{
				bitSet(system_data.sys_byte, SYS_BIT_AUTO_DAYLIGHT);
			}
			else
			{
				bitUnSet(system_data.sys_byte, SYS_BIT_AUTO_DAYLIGHT);
				if (!bitGet(system_data.sys_byte, SYS_BIT_FOG))
				{
					SET_FOG_OFF
				}
			}

			systemSave();
		}
		break;

		// Отправка информации о включениие противотуманки при включении поворотника
		case COM_GET_FERNS_FOG:
		{	
			dataWrite(COM_GET_FERNS_FOG, (bitGet(system_data.sys_byte, SYS_BIT_FERNS_FOG) ? 1 : 0), 0, 0, 2);
		}

		// Установка включениия противотуманки при включении поворотника
		case COM_SET_FERNS_FOG:
		{
			if (dataRead(1) == 1)
				bitSet(system_data.sys_byte, SYS_BIT_FERNS_FOG);
			else
				bitUnSet(system_data.sys_byte, SYS_BIT_FERNS_FOG);

			systemSave();
		}
		break;

		// Отправка информации о режиме дибила
		case COM_GET_DIBILMODE:
		{	
			dataWrite(COM_GET_DIBILMODE, (bitGet(byte_tmp, TMP_BIT_DIBILMODE) ? 1 : 0), 0, 0, 2);
		}

		// Установка режима дибила
		case COM_SET_DIBILMODE:
		{
			if (dataRead(1) == 1)
			{
				bitSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);
				bitSet(byte_tmp, TMP_BIT_DIBILMODE);
			}
			else
			{
				bitUnSet(bytes_shift_register[0], BIT_F_FOG_LIGHTS);
				bitUnSet(byte_tmp, TMP_BIT_DIBILMODE);
			}
		}
		break;

		case COM_GET_DRIVE_MODE_VALUE:
		{
			value = 0;

			if (bitGet(bytes_shift_register[0], BIT_F_HIGHLIGHTS))
				value |= DRV_BIT_HEADLIGHT_ON;

			if (GET_STATUS_FOG)
				value |= DRV_BIT_FOG_ON;

			if (bitGet(byte_tmp, TMP_BIT_FERN_ON))
				value |= DRV_BIT_FOG_FERNS_ON;

			if (state_mirrors_heating == 1)
				value |= DRV_BIT_MIRRORS_HEATING_ON;

			if (state_mirrors_heating == 2)
				value |= DRV_BIT_MIRRORS_HEATING_OFF;

			dataWrite(COM_GET_DRIVE_MODE_VALUE, value, 0, 0, 2);
		}
		break;

		default: dataClear();
	}
}