// #define DEBUG
// #define NO_INTERNET
// #define NO_I2C

#include <Arduino.h>
#include <SPI.h>

#ifndef NO_I2C
#include <Wire.h>
#endif

#ifdef DEBUG
#include <SoftwareSerial.h>
SoftwareSerial debug(10, 11); // RX, TX
#endif

#include "config.h"
#include "rfid.h"
#include "fast_op.h"
#include "sim800.h"

// #define SRV_ADDRESS	"AT+CIPSTART=\"TCP\",\"127.0.0.1\",\"1234\"\0"

#define MAGIC_NUM 600

// Обслуживание зажигания
#define INIT_ACC			bitSet(DDRC, 1)
#define INIT_IGN1			bitSet(DDRC, 2)
#define INIT_IGN2			bitSet(DDRC, 3)
#define INIT_STR			bitSet(DDRB, 0)
#define GET_ACC				bitGet(PORTC, 1)
#define GET_IGN1			bitGet(PORTC, 2)
#define GET_IGN2			bitGet(PORTC, 3)
#define GET_STR				bitGet(PORTB, 0)
#define SET_ON_ACC			bitSet(PORTC, 1)
#define SET_ON_IGN1			bitSet(PORTC, 2)
#define SET_ON_IGN2			bitSet(PORTC, 3)
#define SET_ON_STR			bitSet(PORTB, 0)
#define SET_OFF_ACC			bitUnSet(PORTC, 1)
#define SET_OFF_IGN1		bitUnSet(PORTC, 2)
#define SET_OFF_IGN2		bitUnSet(PORTC, 3)
#define SET_OFF_STR			bitUnSet(PORTB, 0)

// Обслуживание брелока
#define INIT_LOCK			bitSet(DDRD, 6)
#define INIT_UNLOCK			bitSet(DDRD, 7)
#define DISABLE_ALARM		bitSet(PORTD, 7)
#define ENABLE_ALARM		bitUnSet(PORTD, 7)
#define PANIC_ON			bitSet(PORTD, 6)
#define PANIC_OFF			bitUnSet(PORTD, 6)

// Обслуживание ручника
#define INIT_HAND_BREAKER	bitUnSet(PORTC, 0)
#define GET_HAND_BREAKER	bitGet(PINC, 0)

// Обслуживание пищалки
#define INIT_BEEP			bitSet(DDRD, 5);\
							TCCR0A	|= (1 << WGM01) | (1 << WGM00);\
							TCCR0B	|= (1 << CS01)

#define GET_BEEP(v)			TCCR0A |= (1 << COM0B1);\
							OCR0B = 150;\
							_delay_ms(v);\
							OCR0B = 0;\
							TCCR0A &= ~(1 << COM0B1)

// Обслуживание RFID-считывателя
#define PIN_SS				2	// PB
#define PIN_SS_lib			10	// For rfid lib

/*Answer for functions*/
#define CARD_OK				0
#define CARD_NOT_OK			1

/*Sizes*/
#define SIZE_DB 			10	// How many id's will be
#define SIZE_ID 			5	// Bytes in one id

uint8_t old_id[SIZE_ID] = {};		// For one time read card
uint8_t now_id[SIZE_ID] = {};		// For one time read card
uint8_t rfid_now[SIZE_ID] = {};		// RFID for send to server
uint8_t dead_beef = 0;				// Crutch for one time read card
uint8_t settings_byte = 0;			// Setting byte for EEPROM
uint8_t num_of_ID = 0;				// How mach id's we have in EEPROM
uint8_t status = 0;					// For answer rfid module
uint8_t num_of_phone = 0;			// How mach id's we have in EEPROM

volatile uint8_t i2c_buffer[3] = {};
volatile uint8_t i2c_count = 0;

uint64_t timer_starter = 0;
// Таймер для удалённого старта
uint64_t timer_engine_work = 0;

/*BYTE_TMP*/
#define TMP_ENGINE_WORK		0b00000001
#define TMP_ENGINE_STOPED	0b00000010
#define TMP_UNAUTHORIZED	0b00000100
#define TMP_SPEED_SIGNAL	0b00001000
#define TMP_FAIL_STOP		0b00010000
#define TMP_I2C_AVAILABLE	0b00100000
#define TMP_AUTHORIZED		0b01000000
#define TMP_WEATHER_WAIT	0b10000000

volatile uint8_t byte_tmp = 0;
volatile uint16_t rpm = 0;
volatile uint64_t time_rpm = 0;

/*Adress data in EEPROM*/
#define BYTES_SHIFT					0
#define POSITION_SETTINGS_BYTE		BYTES_SHIFT
#define POSITION_NUM_OF_ID			POSITION_SETTINGS_BYTE + 1
#define POSITION_CARD_DB			POSITION_NUM_OF_ID + 1
#define POSITION_NUM_OF_PHONES		POSITION_CARD_DB + 1 + SIZE_DB
#define POSITION_PHONES_DB			POSITION_NUM_OF_PHONES + 1

ISR(INT0_vect)
{
	byte_tmp |= TMP_SPEED_SIGNAL;
}

ISR(INT1_vect)
{
	rpm = 30 / ((float)(micros() - time_rpm) / 1000000);
	time_rpm = micros();

	if (rpm > MAGIC_NUM)
	{
		byte_tmp |= TMP_ENGINE_WORK;
	}
}

SIGNAL(TIMER0_COMPA_vect)
{
	// Сброс количества оборотов, если они не обновлялись полсекунды
	if (((micros() - time_rpm) > 500000) && (byte_tmp & TMP_ENGINE_WORK))
	{
		rpm = 0;
		byte_tmp &= ~(TMP_ENGINE_WORK | TMP_SPEED_SIGNAL);
	}

	//Обработка сообщений с SIM800
	SIM800_read();
}

#ifndef NO_I2C
void slaveRX(int16_t byte)
{
	i2c_buffer[i2c_count] = Wire.read();
	i2c_count++;

	if (i2c_count == 2)
	{
		i2c_count = 0;
		i2c_buffer[2] = 0;
		byte_tmp |= TMP_I2C_AVAILABLE;
	}
}

void slaveTX()
{
	Wire.write(i2c_buffer[2]);
}
#endif

void delete_rfid(uint8_t rfid_num)
{
	uint8_t temp_next_id[SIZE_ID] = {};

	for (uint8_t i = rfid_num; i < num_of_ID; ++i)
	{
		EEPROMReadData(temp_next_id, POSITION_CARD_DB + (SIZE_DB * (i + 1)));
		EEPROMWriteData(temp_next_id, POSITION_CARD_DB + (SIZE_DB * i));
	}

	num_of_ID -= 1;

	EEPROMWriteData(num_of_ID, POSITION_NUM_OF_ID);
}

uint8_t compare(uint8_t *first, uint8_t *second)
{
	for (uint8_t i = 0; i < SIZE_ID; ++i)
	{
		if (first[i] != second[i])
		{
			return 0;
		}
	}

	return 1;
}

uint8_t getID(uint8_t *card_id)
{
	status = MFRC522_Request(PICC_REQIDL, now_id);

	if (status != MI_OK)
	{
		if (dead_beef == 2)
		{
			dead_beef = 0;
			memcpy(old_id, NULL, SIZE_ID);
		}

		dead_beef++;
		return CARD_NOT_OK;
	}
	else
	{
		dead_beef = 0;
	}

	status = MFRC522_Anticoll(now_id);

	if ((compare(now_id, old_id)) || (status != MI_OK))
	{
		return CARD_NOT_OK;
	}

	memcpy(card_id, now_id, SIZE_ID);
	memcpy(old_id, now_id, SIZE_ID);

	return CARD_OK;
}

uint8_t checkID(uint8_t *card_id)
{
	uint8_t check_status = 0;	// For checking correct ID
	uint8_t temp_id;
	uint8_t j = 0;
	uint8_t cell_adress = 0;

	for ( uint8_t i = 0; i < SIZE_ID; i++)
	{
		cell_adress = POSITION_CARD_DB + i + (j * SIZE_ID);

		while (j < num_of_ID)
		{
			EEPROMReadData(temp_id, cell_adress);

			if (card_id[i] == temp_id)
			{
				rfid_now[check_status] = card_id[i];
				check_status++;
				break;
			}

			cell_adress += SIZE_ID;
			j++;
		}
	}

	if (check_status == SIZE_ID)	// If found card ID in our DB then exit of cycle
	{
		return CARD_OK;
	}

	return CARD_NOT_OK;
}

uint8_t checkPhone(const uint8_t *phone)
{
	uint8_t check_status = 0;	// For checking correct status
	uint8_t temp_id;
	uint8_t j = 0;
	uint8_t cell_adress = 0;

	for ( uint8_t i = 0; i < 12; i++)
	{
		cell_adress = POSITION_PHONES_DB + i + (j * 12);

		while (j < num_of_phone)
		{
			EEPROMReadData(temp_id, cell_adress);

			if (phone[i] == temp_id)
			{
				check_status++;
				break;
			}

			cell_adress += 12;
			j++;
		}
	}

	if (check_status == 12)	// If found card ID in our DB then exit of cycle
	{
		return CARD_OK;
	}

	return CARD_NOT_OK;
}

uint8_t check_cards()
{
	uint8_t temp_id[SIZE_ID];

	if (getID(temp_id) == CARD_NOT_OK)
	{
		return CARD_NOT_OK;
	}

	if (checkID(temp_id) == CARD_OK)
	{	
		return CARD_OK;
	}
	else
	{
		GET_BEEP(500);
		_delay_ms(500);
		GET_BEEP(500);
	}
	return CARD_NOT_OK;
}

void add_cards()
{
	uint8_t temp_id[SIZE_ID];

	if (num_of_ID == 255)
	{
		num_of_ID = 0;
	}

	uint8_t temp_num_id = num_of_ID;

	GET_BEEP(250);
	_delay_ms(250);
	GET_BEEP(250);
	_delay_ms(250);
	GET_BEEP(250);

	while (getID(temp_id) != CARD_OK);

	if (checkID(temp_id) != CARD_OK)
	{
		for (uint8_t i = 0; i < SIZE_ID; ++i)
		{
			EEPROMWriteData(temp_id[i], POSITION_CARD_DB + (num_of_ID * SIZE_ID) + i);
		}

		num_of_ID++;
	}
	else
	{
		GET_BEEP(250);
		_delay_ms(250);
		GET_BEEP(250);
	}

	if (temp_num_id != num_of_ID)
	{
		GET_BEEP(1000);
		EEPROMWriteData(num_of_ID, POSITION_NUM_OF_ID);
	}
}

void addPhone(const uint8_t *phoneNumber)
{
	if (num_of_phone == 255)
	{
		num_of_phone = 0;
	}

	uint8_t temp_num_id = num_of_phone;

	if (checkPhone(phoneNumber) != CARD_OK)
	{
		for (uint8_t i = 0; i < 12; ++i)
		{
			EEPROMWriteData(phoneNumber[i], POSITION_PHONES_DB + (num_of_phone * 12) + i);
		}

		num_of_phone++;
	}
	else
	{
		GET_BEEP(250);
		_delay_ms(250);
		GET_BEEP(250);
	}

	if (temp_num_id != num_of_phone)
	{
		GET_BEEP(1000);
		EEPROMWriteData(num_of_phone, POSITION_NUM_OF_ID);
	}
}

uint8_t engine_start(void)
{
	uint8_t state_speed_old = 0;
	uint8_t state_speed = 0;
	uint8_t count_fail = 0;

	// При возникновении серьёзной ошибки запуска не обрабатывать команды запуска двигателя
	if (byte_tmp & TMP_FAIL_STOP)
	{
		return 0;
	}

	// Если производится запуск на заблокированной машине, то отключить сигнализацию
	if ((byte_tmp & TMP_UNAUTHORIZED) && (!(byte_tmp & TMP_ENGINE_WORK)))
	{
		DISABLE_ALARM;
		_delay_ms(1000);
	}

	SET_ON_ACC;
	SET_ON_IGN1;
	SET_ON_IGN2;

	_delay_ms(1000);

	if (byte_tmp & TMP_ENGINE_WORK)
	{
		timer_starter = millis();

		while ((millis() - timer_starter) < 3000)
		{
			if (!(byte_tmp & TMP_ENGINE_WORK))
			{
				timer_starter = 0;
				break;
			}
		}

		if (timer_starter != 0)
		{
			if (byte_tmp & TMP_UNAUTHORIZED)
			{
				timer_starter -= 300000L;
			}

			GET_BEEP(1500);
			return 1;
		}
	}

	state_speed = state_speed_old = (byte_tmp & TMP_SPEED_SIGNAL);

	// Если двигатель был заведён, но заглох
	if (byte_tmp & TMP_ENGINE_STOPED)
	{
		_delay_ms(5000);
	}

	SET_OFF_IGN2;
	SET_ON_STR;

	timer_starter = millis();

	while (!(byte_tmp & TMP_ENGINE_WORK))
	{
		if (state_speed != state_speed_old)
		{
			if (count_fail == 1) {
				GET_BEEP(500);
				_delay_ms(500);
				GET_BEEP(500);
				_delay_ms(500);
				GET_BEEP(500);
				_delay_ms(500);
				GET_BEEP(500);
				byte_tmp |= TMP_ENGINE_STOPED | TMP_FAIL_STOP;
				SET_OFF_ACC;
				SET_OFF_IGN1;
				SET_OFF_IGN2;
				break;
				return 0;
			}
			else
			{
				state_speed = state_speed_old = (byte_tmp & TMP_SPEED_SIGNAL);
				count_fail++;
			}
		}
		// Если не удалось завести двигатель в течении 5 секунд
		if ((millis() - timer_starter) >= 5000)
		{
			SET_OFF_STR;

			if (byte_tmp & TMP_UNAUTHORIZED)
			{
				SET_OFF_ACC;
				SET_OFF_IGN1;
				SET_OFF_IGN2;
				byte_tmp |= TMP_ENGINE_STOPED;
			}
			else
			{
				SET_ON_IGN2;
			}

			GET_BEEP(500);
			_delay_ms(500);
			GET_BEEP(500);

			return 0;
		}

		// Если нет сигнала с тахометра
		if (((millis() - timer_starter) >= 1000) && (rpm == 0))
		{
			SET_OFF_STR;
			SET_ON_IGN2;

			if (byte_tmp & TMP_UNAUTHORIZED)
			{
				SET_OFF_ACC;
				SET_OFF_IGN1;
				SET_OFF_IGN2;
				byte_tmp |= TMP_ENGINE_STOPED;
			}
			else
			{
				SET_ON_IGN2;
			}

			GET_BEEP(500);
			_delay_ms(500);
			GET_BEEP(500);
			_delay_ms(500);
			GET_BEEP(500);

			return 0;
		}

		state_speed_old = (byte_tmp & TMP_SPEED_SIGNAL);
	}

	if (byte_tmp & TMP_UNAUTHORIZED)
	{
		timer_engine_work = millis();
	}

	SET_OFF_STR;
	SET_ON_IGN2;

	byte_tmp |= TMP_ENGINE_WORK;
	byte_tmp &= ~(TMP_ENGINE_STOPED);
	return 1;
}

void engine_stop(void)
{
	SET_OFF_ACC;
	SET_OFF_IGN1;
	SET_OFF_IGN2;
	SET_OFF_STR;
	ENABLE_ALARM;

	byte_tmp &= ~(TMP_ENGINE_WORK);
	byte_tmp |= (TMP_ENGINE_STOPED);
}

void setup()
{
	Serial.begin(115200);

	SPI.begin();

#ifndef NO_I2C
	Wire.begin(0x01);
	Wire.onReceive(slaveRX);
	Wire.onRequest(slaveTX);
#endif

	// Обслуживание RFID считывателя
	bitSet(DDRB, PIN_SS);
	bitUnSet(PORTB, PIN_SS);
	chipSelectPin = PIN_SS_lib;
	MFRC522_Init();

	// Прерывания по таймеру
	OCR0A = 0xAF;
	TIMSK0 |= _BV(OCIE0A);

	// Внешние прерывания со спидометра
	DDRD &= ~(1 << 2);
	EICRA |= (1 << ISC01);	// По спадающему
	EICRA &= ~(1 << ISC00);
	EIMSK |= (1 << INT0);

	// Внешние прерывания с тахометра
	DDRD &= ~(1 << 3);
	EICRA |= (1 << ISC11);	// По спадающему
	EICRA &= ~(1 << ISC10);
	EIMSK |= (1 << INT1);

	INIT_ACC;
	INIT_IGN1;
	INIT_IGN2;
	INIT_STR;
	INIT_LOCK;
	INIT_UNLOCK;
	INIT_BEEP;
	INIT_HAND_BREAKER;

	// Для задержки бензонасоса
	byte_tmp |= TMP_ENGINE_STOPED;

	EEPROMReadData(settings_byte, POSITION_SETTINGS_BYTE);
	EEPROMReadData(num_of_ID, POSITION_NUM_OF_ID);
	EEPROMReadData(num_of_phone, POSITION_NUM_OF_PHONES);

	if (num_of_ID == 255)
	{
		add_cards();
		/*addPhone((const uint8_t*)"+79535363544");*/
	}

#ifdef DEBUG
	debug.begin(115200);
#endif

	GET_BEEP(500);
}

void loop()
{
	if (!GET_STR)
	{
		SIM800_state();

	/*	if (bitGet(byte_sim800_tmp, SIM_TMP_SMS_AVAILABLE))
		{
			if (checkPhone(sms_sender_number) == CARD_OK)
			{
				switch (sms_message[sms_message_length - 1])
				{
					case '0':
					{
						GET_BEEP(1000);
					}
					break;

					case '1':
					{
						if (sms_message_length == 1)
							engine_start();
					}
					break;

					case '2':
					{
						if (sms_message_length == 1)
							engine_stop();
					}
					break;

					case '5':
					{
						if (sms_message_length == 1)
							add_cards();
					}
					break;
				}
			}

			bitUnSet(byte_sim800_tmp, SIM_TMP_SMS_AVAILABLE);
		}*/

		if (bitGet(byte_sim800_tmp, SIM_TMP_SRV_DATA_AVAILABLE))
		{
			switch (srv_buf[0])
			{
				case '0':
				{
					GET_BEEP(1000);
					srv_cache_command = '9';
				}
				break;

				case '1':
				{
					if (!GET_HAND_BREAKER)
					{
						byte_tmp |= TMP_UNAUTHORIZED;

						if (engine_start())
						{
							srv_cache_command = '9';
						}
						else
						{
							engine_stop();
							srv_cache_command = '8';
						}
					}
					else
					{
						GET_BEEP(1000);
						srv_cache_command = '8';
					}

					srv_data_wait = 1;
				}
				break;

				case '2':
				{
					engine_stop();
					byte_tmp &= ~(TMP_AUTHORIZED);
					srv_cache_command = '9';
					srv_data_wait = 1;
				}
				break;

				case '3':
				{
					uint8_t send_byte = 0;

					// Бит работающего двигателя
					if (byte_tmp & TMP_ENGINE_WORK)
						send_byte |= 0b00000001;

					// Бит поднятого ручника
					if (!GET_HAND_BREAKER)
						send_byte |= 0b00000010;

					// Бит аварийной остановки
					if (byte_tmp & TMP_FAIL_STOP)
						send_byte |= 0b00000100;

					srv_cache_command = send_byte;
					srv_data_wait = 1;
				}
				break;

				case '4':
				{
					srv_cache_command = (double)((millis() - timer_engine_work) / 60000);
					srv_data_wait = 1;
				}
				break;

				case '5':
				{
					srv_cache_command = '9';
					srv_data_wait = 1;
					add_cards();
				}
				break;

				case '7':
				{
					uint8_t i = 1;
					uint8_t j = 0;
					uint8_t k = 0;
					int8_t temp_weather = 0;

					if (srv_buf[1] == '-')
						i = 2;

					for (i; i < srv_buf_length; ++i)
					{
						if (srv_buf[1] == '-')
							k = i - 1;
						else
							k = i;

						for (j; j != k; ++j)
							temp_weather *= 10;

						temp_weather += srv_buf[i] - 48;
					}

					if (srv_buf[1] == '-')
						temp_weather *= -1;

					if (byte_tmp & TMP_WEATHER_WAIT)
					{
						if (temp_weather == 0)
							temp_weather = 99;

						i2c_buffer[2] = temp_weather;
						byte_tmp &= ~TMP_WEATHER_WAIT;
					}
				}
				break;
			}

			bitUnSet(byte_sim800_tmp, SIM_TMP_SRV_DATA_AVAILABLE);
		}
	}

	if (byte_tmp & TMP_I2C_AVAILABLE)
	{
		byte_tmp &= ~TMP_I2C_AVAILABLE;

		switch (i2c_buffer[0])
		{
			// Верификация RFID-карты
			case COM_I2C_GET_RFID:
			{
				uint32_t timer_local_rfid = 0;

				i2c_buffer[2] = 0;
				GET_BEEP(1000);

				for (;;)
				{
					if (check_cards() == CARD_OK)
					{
						GET_BEEP(500);
						byte_tmp |= TMP_AUTHORIZED;
						i2c_buffer[2] = 2;
						break;
					}

					if (timer_local_rfid >= 10000)
					{
						GET_BEEP(500);
						_delay_ms(500);
						GET_BEEP(500);
						i2c_buffer[2] = 1;
						break;
					}

					timer_local_rfid++;
					_delay_ms(1);
				}
			}
			break;

			// Управление положением ACC
			case COM_I2C_SET_ACC:
			{
				if (i2c_buffer[1])
					SET_ON_ACC;
				else
				{
					if (!GET_IGN1)
						SET_OFF_ACC;
				}
			}
			break;

			// Запуск двигателя в автоматическом режиме
			case COM_I2C_SET_ENGINE_START:
			{
				if (engine_start())
					i2c_buffer[2] = 2;
				else
					i2c_buffer[2] = 1;
			}
			break;

			// Остановка двигателя
			case COM_I2C_SET_ENGINE_STOP:
			{
				engine_stop();
				i2c_buffer[2] = 2;
			}
			break;

			// Запуск зажигания врручную
			case COM_I2C_SET_MANUAL_START:
			{
				SET_ON_ACC;
				SET_ON_IGN1;
				SET_ON_IGN2;

				i2c_buffer[2] = 2;
			}
			break;

			// Запуск стартера врручную
			case COM_I2C_SET_STARTER:
			{
				if (i2c_buffer[1])
				{
					// uint32_t timer_local_starter = 0;
					SET_OFF_IGN2;
					SET_ON_STR;
					i2c_buffer[2] = 2;
				}
				else
				{
					SET_ON_IGN2;
					SET_OFF_STR;

					if (byte_tmp & TMP_ENGINE_WORK)
						i2c_buffer[2] = 2;
					else
						i2c_buffer[2] = 1;
				}

			}
			break;

			// Если водителя нет в машине
			case COM_I2C_SET_DRIVER_OUT:
			{
				SET_OFF_ACC;
				byte_tmp &= ~TMP_AUTHORIZED;
			}
			break;

			// Получение статуса авторизации
			case COM_I2C_GET_AUTHORIZED:
			{
				if (byte_tmp & TMP_AUTHORIZED)
					i2c_buffer[2] = 2;
				else
					i2c_buffer[2] = 1;
			}
			break;

			case COM_I2C_SET_ADD_RFID:
			{
				add_cards();
			}
			break;

			case COM_I2C_GET_TEMP:
			{
				srv_cache_command = '1';
				byte_tmp |= TMP_WEATHER_WAIT;
				srv_data_wait = 1;
			}
			break;
		}
	}

	if (!GET_STR)
	{
		if (check_cards() == CARD_OK)
		{
			GET_BEEP(500);

			byte_tmp |= TMP_AUTHORIZED;

			if (byte_tmp & TMP_UNAUTHORIZED)
			{
				byte_tmp &= ~(TMP_UNAUTHORIZED);
				timer_engine_work = 0;
				return;
			}

			if (byte_tmp & TMP_FAIL_STOP)
			{
				byte_tmp &= ~TMP_FAIL_STOP;
				GET_BEEP(2000);
				_delay_ms(1000);
				GET_BEEP(2000);
				_delay_ms(1000);
				GET_BEEP(2000);
				_delay_ms(5000);
			}

			if ((byte_tmp & TMP_ENGINE_WORK) && (GET_IGN1))
			{
				engine_stop();
				byte_tmp &= ~(TMP_AUTHORIZED);
			}
			else
			{
				engine_start();
			}
		}

		if ((byte_tmp & TMP_UNAUTHORIZED) && ((millis () - timer_engine_work) >= 1200000L))
		{
			byte_tmp &= ~(TMP_UNAUTHORIZED);
			byte_tmp &= ~TMP_AUTHORIZED;

			engine_stop();
		}

		if ((byte_tmp & TMP_UNAUTHORIZED) && (GET_HAND_BREAKER))
		{
			byte_tmp &= ~(TMP_UNAUTHORIZED);
			engine_stop();
		}
	}
}