#include <Arduino.h>				// Для работы Serial
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"

Adafruit_SSD1306 display(4);

// Resistors
#define RES1 10000.0
#define RES2 33000.0

// Delays
#define DELAY_DOOR_CLOSED 15000

// PINS
#define PIN_B_ENC_B			0b00000001
#define PIN_B_ENC_OK		0b00010000
#define PIN_D_IGNITION		0b00000100
#define PIN_D_TACHOMETR		0b00001000
#define PIN_D_DOOR			0b00010000
// #define PIN_D_SPEEDOMETR	0b00100000
#define PIN_D_ENC_A			0b10000000
#define APIN_TEMP_ENGINE	0b00000010
#define APIN_TEMP_BATTERY	0b00001000

// Encoder value
#define NONE		0
#define ENC_OK		1
#define ENC_UP		2
#define ENC_DOWN	3

// System state
#define STATE_WAIT	0
#define STATE_MENU	1
#define STATE_LIGHT	2
#define STATE_TEMP	3
#define STATE_TIME	4
#define STATE_SLEEP	5
#define STATE_DRIVE	6

// system byte
#define SYS_BIT_SENDED_WORK_ENGINE	0b00000001
#define SYS_BIT_ENGINE_WORK			0b00000010
#define SYS_BIT_WAIT_CLOSE_DOOR		0b00000100
#define SYS_BIT_DRIVER_INSIDE		0b00001000
#define SYS_BIT_AUTOSTOP_ENGINE		0b00010000

// Temp bits
#define TMP_BIT_NO_NEED_SLEEP		0b00000001
#define TMP_DATA_BUSY				0b00000010
#define TMP_SEND_DRIVER_INSIDE		0b00000100
#define TMP_BIT_NO_NEED_AUTOSTOP	0b00001000
#define TMP_BIT_ENGINE_STOP			0b00010000
#define TMP_BIT_WEATHER_PRINT		0b00100000

volatile uint8_t byte_sys = 0,
		byte_tmp = 0,
		state_sys = STATE_WAIT,
		encoder_A_prev = 0,
		protocol_array[5] = {},
		i2c_buffer[3] = {},
		count_open_door = 0;			// Счётчик открытия дверей

volatile  uint16_t rpm = 0;

volatile uint64_t time_rpm = 0,
		time_door = 0;	// Время последнего закрытия двери

static const uint8_t PROGMEM icon_mirrors_heating[] = {
	B00000111, B11111110,
	B00001000, B00000001,
	B00001000, B10010001,
	B11111000, B11110001,
	B11101000, B10010001,
	B11001000, B00000001,
	B10000111, B11111110,
	B00000000, B00000000
};

static const uint8_t PROGMEM icon_headligh[] = {
	B00000000, B01111000,
	B00001110, B11111100,
	B00111000, B11111110,
	B11100000, B11111111,
	B00000000, B11111111,
	B00001110, B11111110,
	B00111000, B11111100,
	B11100000, B01111000
};

static const uint8_t PROGMEM icon_fog[] = {
	B00110000, B01111000,
	B11000000, B10000100,
	B00110000, B10000110,
	B00001100, B10011111,
	B00110000, B10001111,
	B11000000, B10001110,
	B00110000, B10011100,
	B00001100, B01111000
};

ISR(INT0_vect)
{

}

// Прерывание с тахометра
ISR(INT1_vect)
{
	rpm = 30 / ((float)(micros() - time_rpm) / 1000000);
	time_rpm = micros();

	if (rpm > 800)
		byte_sys |= SYS_BIT_ENGINE_WORK;
}

SIGNAL(TIMER0_COMPA_vect)
{
	// Если ранее был включен двигатель, но сейчас ключ не в положении зажигания
	if ((byte_sys & SYS_BIT_ENGINE_WORK) &&  (!(PIND & PIN_D_IGNITION)))
		byte_sys &= ~(SYS_BIT_ENGINE_WORK);

	// Сброс счётчика оборотов, если импульса нет полсекунды
	if (byte_sys & SYS_BIT_ENGINE_WORK)
	{
		if ((micros() - time_rpm) > 500000)
		{
			rpm = 0;
			byte_sys &= ~(SYS_BIT_ENGINE_WORK);
		}
	}

	// Если водитель внутри и двигатель работает
	if ((byte_sys & SYS_BIT_DRIVER_INSIDE) && (byte_sys & SYS_BIT_ENGINE_WORK))
	{
		if (!(PIND & PIN_D_DOOR))
		{
			byte_sys |= SYS_BIT_WAIT_CLOSE_DOOR;
		}

		if ((PIND & PIN_D_DOOR) && (byte_sys & SYS_BIT_WAIT_CLOSE_DOOR))
		{
			byte_sys &= ~(SYS_BIT_WAIT_CLOSE_DOOR);

			if (byte_tmp & TMP_BIT_NO_NEED_AUTOSTOP)
			{
				count_open_door++;

				if (count_open_door == 2)
				{
					byte_tmp &= ~(TMP_BIT_NO_NEED_AUTOSTOP);
					count_open_door = 0;
				}
			}
			else
			{
				byte_tmp |= TMP_BIT_ENGINE_STOP;
			}
		}
	}

	// Если мозги не врежиме сна и ключ не в положении зажигания
	if ((state_sys != STATE_SLEEP) && (!(PIND & PIN_D_IGNITION)))
	{
		// Если дверь открывается, то водитель вышел
		if (!(PIND & PIN_D_DOOR))
		{
			state_sys = STATE_SLEEP;
			byte_sys &= ~(SYS_BIT_DRIVER_INSIDE);
			byte_tmp &= ~(TMP_SEND_DRIVER_INSIDE | TMP_BIT_NO_NEED_SLEEP | TMP_BIT_WEATHER_PRINT);
			time_door = millis();
		}
	}

	if (byte_sys & SYS_BIT_WAIT_CLOSE_DOOR)
	{
		if (PIND & PIN_D_DOOR)
		{
			if (byte_sys & SYS_BIT_DRIVER_INSIDE)
			{
				byte_sys &= ~(SYS_BIT_DRIVER_INSIDE);
				byte_tmp |= TMP_SEND_DRIVER_INSIDE;
			}
			else
			{
				state_sys = STATE_WAIT;
				byte_sys |= SYS_BIT_DRIVER_INSIDE;
				byte_tmp &= ~(TMP_SEND_DRIVER_INSIDE);
			}

			byte_tmp &= ~(TMP_BIT_WEATHER_PRINT);
			byte_sys &= ~(SYS_BIT_WAIT_CLOSE_DOOR);
		}
	}

	// Отправка второму мозгу информации о наличии водителя в машине
	if ((byte_sys & SYS_BIT_DRIVER_INSIDE) && (!(byte_tmp & TMP_SEND_DRIVER_INSIDE)))
	{
		if (!(byte_tmp & TMP_DATA_BUSY))
		{
			dataWrite(COM_SET_DRIVER_INSIDE, 0, 0, 0, 1);
			byte_tmp |= TMP_SEND_DRIVER_INSIDE;
		}
	}

	// Если водителя нет в машине и после закрытия двери прошло DELAY_DOOR_CLOSED времени
	if ((!(byte_sys & SYS_BIT_DRIVER_INSIDE)) && ((millis() - time_door) >= DELAY_DOOR_CLOSED))
		if (!(PIND & PIN_D_DOOR))
			byte_sys |= SYS_BIT_WAIT_CLOSE_DOOR;
}

void setup(void)
{
	Wire.begin();
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
	display.clearDisplay();
	display.display();
	Serial.begin(9600);

	display.setTextColor(WHITE);
	display.setTextSize(3);
	display.clearDisplay();
	display.setCursor(0, 0);
	display.print("LOADING");
	display.display();

	DDRB &= ~(PIN_B_ENC_B | PIN_B_ENC_OK);
	DDRD &= ~(PIN_D_IGNITION | PIN_D_TACHOMETR | PIN_D_DOOR | PIN_D_ENC_A);
	// DDRD |= PIN_D_SPEEDOMETR;

	// Прерывания по таймеру
	OCR0A = 0xAF;
	TIMSK0 |= _BV(OCIE0A);

	// Внешние прерывания с датчика оборотов
	/*EICRA &= ~(1 << ISC11);	// По изменению
	EICRA |= (1 << ISC10);*/
	/*EICRA &= ~(1 << ISC11);	// По низкому
	EICRA &= ~(1 << ISC10);*/
	/*EICRA |= (1 << ISC11);	// По нарастающему
	EICRA |= (1 << ISC10);*/
	EICRA |= (1 << ISC11);	// По спадающему
	EICRA &= ~(1 << ISC10);
	EIMSK |= (1 << INT1);
	sei();

	// Настройка АЦП
	// Включаем АЦП
	ADCSRA |= (1 << ADEN)
	// Пределитель на 8
			| (1 << ADPS1) | (1 << ADPS0);
	// Опорное напряжение VCC
	ADMUX |= (1 << REFS0);
	ADMUX &=  ~(1 << REFS1);

	byte_sys |= SYS_BIT_DRIVER_INSIDE;

	_delay_ms(1000);

	dataWrite(COM_SET_DRIVER_INSIDE, 0, 0, 0, 1);
}

void i2cWrite(uint8_t command, uint8_t data)
{	
	Wire.beginTransmission(0x01);
	Wire.write(command);
	Wire.endTransmission();
	Wire.beginTransmission(0x01);
	Wire.write(data);
	Wire.endTransmission();
	i2c_buffer[0] = 0;
}

uint8_t i2cRead(uint16_t timer)
{
	uint16_t timer_local_i2c = 0;
	i2c_buffer[2] = 0;

	for (;;)
	{
		if ((timer != 0) && (timer_local_i2c >= timer))
		{
			return 0;
		}

		Wire.requestFrom(0x01, 1);

		if (Wire.available())
		{
			i2c_buffer[2] = Wire.read();

			if (i2c_buffer[2] != 0)
			{
				return i2c_buffer[2];
			}
		}

		for (uint8_t i = 0; i < 100; ++i)
		{
			_delay_ms(1);
			timer_local_i2c++;
		}
	}

	return 0;
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
	uint8_t count = 0;
	uint16_t time_data = 0;

	while (!Serial.available())
	{
		if (time_data >= 1000)
		{
			dataClear();
			return;
		}

		time_data++;
		_delay_ms(1);
	}

	count = Serial.read();

	for (uint8_t i = 0; i < count; ++i)
	{
		time_data = 0;

		while (!Serial.available())
		{
			if (time_data >= 1000)
			{
				dataClear();
				return;
			}

			time_data++;
			_delay_ms(1);
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
	byte_tmp |= TMP_DATA_BUSY;

	protocol_array[0] = elem0;
	protocol_array[1] = elem1;
	protocol_array[2] = elem2;
	protocol_array[3] = elem3;
	protocol_array[4] = count;

	writeCommand();

	byte_tmp &= ~(TMP_DATA_BUSY);
}

void dataClear(void)
{
	protocol_array[0] = 0;
	protocol_array[1] = 0;
	protocol_array[2] = 0;
	protocol_array[3] = 0;
}

void weatherPrint(uint16_t timer_local_weather)
{
	i2cWrite(COM_I2C_GET_TEMP, 1);
	int8_t weather_temp = i2cRead(2000);

	if (weather_temp != 0)
	{	
		display.clearDisplay();
		display.setTextSize(4);
		display.setCursor(0, 0);

		if (weather_temp == 99)
			display.print('0');
		else
			display.print(weather_temp);

		display.print(" t");
		display.display();
	}

	if (timer_local_weather != 0)
	{
		for (uint32_t i = 0; i < timer_local_weather; ++i)
		{
			if (readEncoder() == ENC_OK)
				break;

			_delay_ms(1);
		}
	}
}

uint8_t readEncoder(void)
{
	uint8_t encoder_A = PIND & PIN_D_ENC_A;
	uint8_t encoder_B = PINB & PIN_B_ENC_B;

	if (PINB & PIN_B_ENC_OK)
	{
		while (PINB & PIN_B_ENC_OK);
		return ENC_OK;
	}

	// Если состояние изменилось с положительного к нулю
	if ((!encoder_A) && (encoder_A_prev)) {
		// Сохраняем значение А для следующего цикла
		encoder_A_prev = encoder_A;

		// Выход В в полож. сост., значит вращение по часовой стрелке
		if(encoder_B)
		{
			return ENC_DOWN;
		}
		else
		{
			return ENC_UP;
		}
	}

	// сохраняем значение А для следующего цикла
	encoder_A_prev = encoder_A;
	return NONE;
}

void stateSleep(void)
{
	byte_sys &= ~(SYS_BIT_DRIVER_INSIDE);
	dataWrite(COM_SET_LOWPOWER_MODE, 0, 0, 0, 1);
	i2cWrite(COM_I2C_SET_DRIVER_OUT, 1);
	display.clearDisplay();
	display.display();

	count_open_door = 0;
	byte_tmp &= ~(TMP_BIT_NO_NEED_AUTOSTOP);

	while (state_sys == STATE_SLEEP)
	{
		if (readEncoder() == ENC_OK)
		{
			state_sys = STATE_WAIT;
			byte_sys |= SYS_BIT_DRIVER_INSIDE;
			byte_tmp |= TMP_BIT_NO_NEED_SLEEP;
			dataWrite(COM_SET_DRIVER_INSIDE, 0, 0, 0, 1);
			break;
		}

		if (PIND & PIN_D_IGNITION)
		{
			state_sys = STATE_WAIT;
			break;
		}
	}

	dataWrite(COM_SET_NORMAL_MODE, 0, 0, 0, 1);
}

void stateDrive(void)
{
	uint64_t delay = 0;
	uint8_t enc_res = 0;
	uint8_t value = 0;
	uint8_t value_drive = 0;
	uint8_t ferns_count = 0;
	uint8_t mirrors_count = 0;

	uint64_t value_drive_time = millis();

	byte_tmp |= TMP_BIT_NO_NEED_SLEEP;

	while ((PIND & PIN_D_IGNITION) && (state_sys == STATE_DRIVE))
	{
		if ((byte_sys & SYS_BIT_DRIVER_INSIDE) && (!(byte_tmp & TMP_BIT_WEATHER_PRINT)))
		{
			weatherPrint(5000);
			byte_tmp |= (TMP_BIT_WEATHER_PRINT);
		}

		display.setTextSize(4);
		display.clearDisplay();
		display.setCursor(0,0);
		if (!(byte_tmp & TMP_BIT_NO_NEED_AUTOSTOP))
		{
			display.print(rpm);

			if (value_drive & DRV_BIT_HEADLIGHT_ON)
			{
				display.drawBitmap(112, 12, icon_headligh, 16, 8, 1);
			}

			if ((value_drive & DRV_BIT_FOG_ON) && (!(value_drive & DRV_BIT_FOG_FERNS_ON)))
			{
				display.drawBitmap(112, 23, icon_fog, 16, 8, 1);
			}

			if (value_drive & DRV_BIT_FOG_FERNS_ON)
			{
				if (ferns_count <= 1)
				{
					display.drawBitmap(112, 23, icon_fog, 16, 8, 1);
				}

				if (ferns_count == 2)
					ferns_count = 0;

				ferns_count++;
			}
			
			if (value_drive & DRV_BIT_MIRRORS_HEATING_ON)
			{
				display.drawBitmap(112, 0, icon_mirrors_heating, 16, 8, 1);
			}
			
			if (value_drive & DRV_BIT_MIRRORS_HEATING_OFF)
			{
				if (mirrors_count <= 1)
				{
					display.drawBitmap(112, 0, icon_mirrors_heating, 16, 8, 1);
				}

				if (mirrors_count == 2)
					mirrors_count = 0;

				mirrors_count++;
			}
		}
		else
		{
			display.print("Paused");
		}

		display.display();

		delay = millis();

		while ((millis() - delay) < 250)
		{

			if (byte_tmp & TMP_BIT_ENGINE_STOP)
			{
				i2cWrite(COM_I2C_SET_ENGINE_STOP, 1);
				if (i2cRead(3000) == 2)
				{
					i2cWrite(COM_I2C_SET_DRIVER_OUT, 1);

					byte_tmp &= ~(TMP_BIT_ENGINE_STOP);
					byte_sys &= ~(SYS_BIT_ENGINE_WORK);
					state_sys = STATE_SLEEP;
					return;
				}
			}

			// Включение/Отключение иконок
			if ((millis() - value_drive_time) >= 500)
			{
				value_drive_time = millis();
				dataWrite(COM_GET_DRIVE_MODE_VALUE, 0, 0, 0, 1);
				dataWait();
				value_drive = dataRead(1);
			}

			enc_res = readEncoder();

			switch (enc_res)
			{
				case ENC_OK:
				{
					state_sys = STATE_MENU;
					return;
				}
				break;

				case ENC_UP:
				{
					edit_num_update(COM_GET_FAN, 0, 4, 1);
				}
				break;

				case ENC_DOWN:
				{
					edit_num_update(COM_GET_FAN, 0, 4, 1);
				}
				break;
			}
		}
	}

	if (state_sys != STATE_MENU)
		state_sys = STATE_WAIT;
}

void stateWait(void)
{
	uint16_t delay = 0;
	uint8_t hours = 0;
	uint8_t mins = 0;
	uint32_t time_no_action = 0;

	if (byte_sys & SYS_BIT_ENGINE_WORK)
	{
		state_sys = STATE_DRIVE;
		return;
	}

	if ((!(byte_sys & SYS_BIT_ENGINE_WORK)) && (byte_sys &  SYS_BIT_SENDED_WORK_ENGINE))
	{
		dataWrite(COM_SET_ENGINE_NOT_WORK, 0, 0, 0, 1);
		byte_sys &= ~(SYS_BIT_SENDED_WORK_ENGINE);
	}

	while (state_sys == STATE_WAIT)
	{
		if ((byte_sys & SYS_BIT_DRIVER_INSIDE) && (!(byte_tmp & TMP_BIT_WEATHER_PRINT)))
		{
			weatherPrint(5000);
			byte_tmp |= (TMP_BIT_WEATHER_PRINT);
		}

		dataWrite(COM_GET_TIME, 0, 0, 0, 1);
		dataWait();

		hours = dataRead(1);
		mins = dataRead(2);

		display.setTextSize(4);
		display.clearDisplay();
		display.setCursor(0, 0);

		if (hours < 10)
		{
			display.print("0");
		}
			
		display.print(hours);
		display.print(":");

		if (mins < 10)
		{
			display.print("0");
		}

		display.print(mins);
		display.display();

		for (delay = 0; delay < 3000; ++delay)
		{
			if ((!(byte_tmp & TMP_BIT_NO_NEED_SLEEP)) && (time_no_action >= 120000))
			{
				state_sys = STATE_SLEEP;
				return;
			}

			if (readEncoder() == ENC_OK)
			{
				byte_tmp |= TMP_BIT_NO_NEED_SLEEP;
				state_sys = STATE_MENU;
				break;
			}

			if (byte_sys & SYS_BIT_ENGINE_WORK)
			{
				dataWrite(COM_SET_ENGINE_WORK, 0, 0, 0, 1);
				byte_sys |= SYS_BIT_SENDED_WORK_ENGINE;
				state_sys = STATE_DRIVE;
				break;
			}

			time_no_action++;
			delay++;
			_delay_ms(1);
		}
	}
}

// Редактирование числа
uint8_t edit_num(uint8_t min, uint8_t max, uint8_t def, uint8_t inc_num)
{
	uint8_t value = def;
	uint8_t key = 0;

	display.setTextSize(4);

	for (;;)
	{
		display.clearDisplay();
		display.setCursor(0, 0);
		display.print(value);
		display.display();

		while ((key = readEncoder()) == NONE);

		switch (key)
		{
			case ENC_OK:
			{
				return value;
			}
			break;

			case ENC_UP:
			{
				if (value < max)
				{
					if ((value + inc_num) > max)
					{
						value = max;
					}
					else
					{
						value += inc_num;
					}
				}
			}
			break;

			case ENC_DOWN:
			{
				if (value > min)
				{
					if ((value - inc_num) < min)
					{
						value = min;
					}
					else
					{
						value -= inc_num;
					}
				}
			}
			break;
		}
	}

	return 0;
}

// Редактирование числа
void edit_num_update(uint8_t command, uint8_t min, uint8_t max, uint8_t inc_num)
{
	dataWrite(command, 0, 0, 0, 1);
	dataWait();
	uint8_t key = 0;
	uint8_t def = dataRead(1);
	uint8_t value = def;
	uint64_t delay = 0;
	display.setTextSize(4);

	for (;;)
	{
		display.clearDisplay();
		display.setCursor(0, 0);
		display.print(value);
		display.display();

		delay = millis();
		while ((key = readEncoder()) == NONE)
		{
			if (state_sys == STATE_DRIVE)
			{
				if ((millis() - delay) > 2000)
				{
					return;
				}
			}
			else
			{
				if ((millis() - delay) > 5000)
				{
					return;
				}
			}
		}

		switch (key)
		{
			case ENC_OK:
			{
				return;
			}
			break;

			case ENC_UP:
			{
				if (value < max)
				{
					if ((value + inc_num) > max)
					{
						value = max;
						dataWrite((command + 1), value, 0, 0, 2);
					}
					else
					{
						value += inc_num;
						dataWrite((command + 1), value, 0, 0, 2);
					}
				}
			}
			break;

			case ENC_DOWN:
			{
				if (value > min)
				{
					if ((value - inc_num) < min)
					{
						value = min;
						dataWrite((command + 1), value, 0, 0, 2);
					}
					else
					{
						value -= inc_num;
						dataWrite((command + 1), value, 0, 0, 2);
					}
				}
			}
			break;
		}
	}
}

uint8_t printMenu(char list_array[][9], uint8_t all_element)
{
	uint8_t	selected		= 0,
			on_display		= 0,
			key = 0;

	uint16_t timer = 0;

	display.setTextSize(2);

	for (;;)
	{
		timer = 0;
		display.clearDisplay();

		if (selected == on_display)
		{
			display.setCursor(0, 0);
			display.print(">");
			display.setCursor(0,17); 
			display.print(" ");
		}
		else
		{
			display.setCursor(0, 0);
			display.print(" ");
			display.setCursor(0, 17); 
			display.print(">");
		}

		display.setCursor(12, 0);
		display.print(list_array[on_display]);

		if (on_display + 1 < all_element)
		{
			display.setCursor(12, 17);
			display.print(list_array[on_display + 1]);
		}

		display.display();

		while ((key = readEncoder()) == NONE)
		{
			delay(1);
			timer++;

			if (byte_sys & SYS_BIT_ENGINE_WORK)
			{
				if (timer >= 5000)
				{
					state_sys = STATE_WAIT;
					return 0;
				}
			}
			else
			{
				if (timer >= 10000)
				{
					state_sys = STATE_WAIT;
					return 0;
				}
			}
		}

		switch (key)
		{
			case ENC_OK:
			{
				return ++selected;
			}
			break;

			case ENC_DOWN:
			{
				if (selected != 0)
				{
					if (selected == on_display)
					{
						on_display	-= 2;
					}

					selected--;
				}
			}
			break;

			case ENC_UP:
			{
				if (selected < all_element - 1)
				{
					if (selected == (on_display + 1))
					{
						on_display	+= 2;
					}

					selected++;
				}
				break;
			}
		}
	}

	return 0;
}

void settings_menu(uint8_t command_get, uint8_t min, uint8_t max)
{
	dataWrite(command_get, 0, 0, 0, 1);
	dataWait();
	dataWrite((command_get + 1), edit_num(min, max, dataRead(1), 1), 0, 0, 2);
}

void menuTime(void)
{
	char menu_lst[][9] = {
		"Set time",
		"Back"
	};

	switch (printMenu(menu_lst, 2))
	{
		case 1:
		{
			uint8_t hours = 0;
			uint8_t mins = 0;

			display.clearDisplay();
			display.setTextSize(4);
			display.setCursor(0, 0);
			display.print("Hours");
			display.display();
			_delay_ms(1000);
			hours = edit_num(0, 23, 0, 1);		// Часы
			display.clearDisplay();
			display.setTextSize(3);
			display.setCursor(0, 0);
			display.print("Minutes");
			display.display();
			_delay_ms(1000);
			mins = edit_num(0, 59, 0, 1);		// Минуты
			dataWrite(COM_SET_TIME, hours, mins, 0, 3);
		}

		default: return;
	}
}

void menuFan(void)
{
	char menu_lst[][9] = {
		"Status",
		"Auto On",
		"Back"
	};

	switch (printMenu(menu_lst, 3))
	{
		case 1:
		{
			edit_num_update(COM_GET_FAN, 0, 4, 1);
		}
		break;

		case 2:
		{
			settings_menu(COM_GET_FAN_AUTO, 0, 1);
		}
		break;

		default: return;
	}
}

void menuSaloonLight(void)
{
	uint8_t light_on = 0;
	char menu_lst[][9] = {
		"Bright", // Яркость
		"TempBlk",
		"PanelBlk",
		"LegsBlk",
		"GlassBlk",
		"Back"
	};

	for (;;)
	{
		dataWrite(COM_GET_LIGHT_SALOON, 0, 0, 0, 1);
		dataWait();
		light_on = dataRead(1);

		switch (printMenu(menu_lst, 6))
		{
			case 1:
			{
					edit_num_update(COM_GET_LIGHT_BRIGHT, 0, 250, 5);
			}
			break;

			case 2:
			{
					if (light_on & (1 << 0))
					{
						light_on &= ~(1 << 0);
					}
					else
					{
						light_on |= (1 << 0);
					}

					dataWrite(COM_SET_LIGHT_SALOON, light_on, 0, 0, 2);
			}
			break;

			case 3:
			{
					if (light_on & (1 << 1))
					{
						light_on &= ~(1 << 1);
					}
					else
					{
						light_on |= (1 << 1);
					}

					dataWrite(COM_SET_LIGHT_SALOON, light_on, 0, 0, 2);
			}
			break;

			case 4: {
					if (light_on & (1 << 2))
					{
						light_on &= ~(1 << 2);
					}
					else
					{
						light_on |= (1 << 2);
					}

					dataWrite(COM_SET_LIGHT_SALOON, light_on, 0, 0, 2);
			}
			break;

			case 5:
			{
					if (light_on & (1 << 3))
					{
						light_on &= ~(1 << 3);
					}
					else
					{
						light_on |= (1 << 3);
					}

					dataWrite(COM_SET_LIGHT_SALOON, light_on, 0, 0, 2);
			}
			break;

			default: return;
		}
	}
}

void menuHeadlight(void)
{
	char menu_lst[][9] = {
		"Auto On",
		"Hour On",
		"Hour Off",
		"Back"
	};

	for (;;)
	{
		switch (printMenu(menu_lst, 4))
		{
			case 1:
			{
				settings_menu(COM_GET_AUTO_HEADLIGHT, 0, 1);
			}
			break;

			case 2:
			{
				dataWrite(COM_SET_HEADLIGHT_TIME_ON, edit_num(0, 23, 0, 1), 0, 0, 2);
			}
			break;

			case 3:
			{
				dataWrite(COM_SET_HEADLIGHT_TIME_OFF, edit_num(0, 23, 0, 1), 0, 0, 2);
			}
			break;

			default: return;
		}
	}
}

void menuFoglight(void)
{
	char menu_lst[][9] = {
		"Status",
		"Fog Day",	// Противотуманки как дневные ходовые огни
		"FernsFog",
		"Back"
	};

	switch (printMenu(menu_lst, 4))
	{
		case 1:
		{
			edit_num_update(COM_GET_FOG, 0, 1, 1);
		}
		break;

		case 2:
		{
			settings_menu(COM_GET_AUTO_DAYLIGHT, 0, 1);
		}
		break;

		case 3:
		{
			settings_menu(COM_GET_FERNS_FOG, 0, 1);
		}
		break;

		default: return;
	}
}

void menuLight(void)
{
	char menu_lst[][9] = {
		"Saloon",
		"HeadLamp",
		"Fog",
		"DibilMod",
		"Back"
	};

	switch (printMenu(menu_lst, 5))
	{
		case 1:
		{
			menuSaloonLight();
		}
		break;

		case 2:
		{
			menuHeadlight();
		}
		break;

		case 3:
		{
			menuFoglight();
		}
		break;

		case 4:
		{
			edit_num_update(COM_GET_DIBILMODE, 0, 1, 1);
		}
		break;

		default: return;
	}
}

void menuInfo(void)
{
	uint16_t value = 0;
	float vOut = 0;

	for (;;)
	{
		ADMUX	|= (APIN_TEMP_ENGINE << 4);	// С какого порта снимать значения
		ADCSRA	|= (1 << ADSC);	// Начинаем преобразование 

		while (!(ADCSRA & (1 << ADIF)));	// Пока не будет окончано преобразоване

		value = (ADCL | ADCH << 8);
		vOut = ((value * 5.0) / 1024.0) / (float)(RES2 / (RES1 + RES2));

		display.setTextSize(3);
		display.clearDisplay();
		display.setCursor(0, 0);
		display.print(vOut);
		display.display();

		_delay_ms(500);
	}
}

void menuTemperature(void)
{
	char menu_lst[][9] = {
		"Auto",
		"Mirrors",
		"MirTime",
		"Window",
		"Back"
	};

	switch (printMenu(menu_lst, 5))
	{
		case 2:
		{
			edit_num_update(COM_GET_MIRROR_HEATING, 0, 1, 1);
		}
		break;

		case 3:
		{
			uint8_t time_on = 0;
			uint8_t time_off = 0;

			display.clearDisplay();
			display.setTextSize(2);
			display.setCursor(0, 0);
			display.print("Time ON");
			display.display();
			_delay_ms(1000);
			time_on = edit_num(1, 60, 1, 1);
			display.clearDisplay();
			display.setTextSize(2);
			display.setCursor(0, 0);
			display.print("Time OFF");
			display.display();
			_delay_ms(1000);
			time_off = edit_num(1, 60, 1, 1);
			dataWrite(COM_SET_AUTO_MIRROR_TIME, time_on, time_off, 0, 3);
		}
		break;

		case 4:
		{
			edit_num_update(COM_GET_WINDOW_HEATING, 0, 1, 1);
		}
		break;

		default: return;
	}
}

void menuAbout(void)
{
	display.clearDisplay();
	display.setTextSize(1);
	display.setCursor(0, 0);
	display.print("LanOS ");
	display.print(__TIME__);
	display.display();

	for (;;)
	{
		if (readEncoder() == ENC_OK)
		{
			return;
		}
	}
}

void menuEngine(void)
{
	char menu_lst[][9] = {
		"AutoStop",
		"Start",
		"Stop",
		"ACC",
		"Add RFID",
		"MStart",
		"Back"
	};

	display.clearDisplay();

	i2cWrite(COM_I2C_GET_AUTHORIZED, 0);

	// Если пользователь не авторизирован
	if (i2cRead(2000) != 2)
	{
		i2c_buffer[2] = 0;
		// Запросить RFID-карту
		display.setTextSize(2);
		display.setCursor(0, 0);
		display.print("Get RFID");
		display.display();

		i2cWrite(COM_I2C_GET_RFID, 0);

		if (i2cRead(10000) != 2)
			return;
	}

	if (i2c_buffer[2] != 2)
	{
		return;
	}

	switch (printMenu(menu_lst, 7))
	{
		case 1:
		{
			char menu_lst[][9] = {
				"Pause",
				"Setting",
				"Back"
			};

			switch (printMenu(menu_lst, 6))
			{
				case 1:
				{
					byte_tmp |= TMP_BIT_NO_NEED_AUTOSTOP;
					state_sys = STATE_DRIVE;
				}
				break;

				case 2:
				{
					if (edit_num(0, 1, 0, 1))
						byte_sys &= ~(SYS_BIT_AUTOSTOP_ENGINE);
					else
						byte_sys |= SYS_BIT_AUTOSTOP_ENGINE;
				}
				break;

				default: return;
			}

			return;
		}
		break;

		case 2:
		{
			i2cWrite(COM_I2C_SET_ENGINE_START, 1);

			if (i2cRead(0) == 2)
			{
				byte_sys &= ~(SYS_BIT_SENDED_WORK_ENGINE);
				byte_tmp |= TMP_SEND_DRIVER_INSIDE;

				dataWrite(COM_SET_ENGINE_WORK, 0, 0, 0, 1);
				_delay_ms(200);
				dataWrite(COM_SET_DRIVER_INSIDE, 0, 0, 0, 1);

				state_sys = STATE_DRIVE;
			}
		}
		break;

		case 3:
		{
			i2cWrite(COM_I2C_SET_ENGINE_STOP, 0);
			i2cRead(3000);
			dataWrite(COM_SET_ENGINE_NOT_WORK, 0, 0, 0, 1);

			state_sys = STATE_DRIVE;
			return;
		}
		break;

		case 4:
		{
			i2cWrite(COM_I2C_SET_ACC, edit_num(0, 1, 0, 1));
		}
		break;

		// Добавление RFID-карты
		case 5:
		{
			display.clearDisplay();
			display.setTextSize(2);
			display.setCursor(0, 0);
			display.print("Add RFID");
			display.display();

			i2cWrite(COM_I2C_SET_ADD_RFID, 1);
		}
		break;

		// Запуск двигателя вручную
		case 6:
		{
			i2cWrite(COM_I2C_SET_MANUAL_START, 1);

			if (i2cRead(3000) == 2)
			{
				display.clearDisplay();
				display.setTextSize(1);
				display.setCursor(0, 0);
				display.print("Press button");
				display.display();

				while (!(PINB & PIN_B_ENC_OK));

				_delay_ms(500);
				i2cWrite(COM_I2C_SET_STARTER, 1);

				if (i2cRead(2000) == 2)
				{
					while (PINB & PIN_B_ENC_OK);

					i2cWrite(COM_I2C_SET_STARTER, 0);

					if (i2cRead(2000) == 2)
					{
						byte_sys &= ~(SYS_BIT_SENDED_WORK_ENGINE);
						byte_tmp |= TMP_SEND_DRIVER_INSIDE;

						dataWrite(COM_SET_ENGINE_WORK, 0, 0, 0, 1);
						dataWrite(COM_SET_DRIVER_INSIDE, 0, 0, 0, 1);
					}
				}
			}
		}
		break;

		default: return;
	}
}

void menuMain(void)
{
	char menu_lst[][9] = {
		"Engine",
		"Fan",
		"Light",
		"Temp",
		"Time",
		"Info",
		"About",
		"Back"
	};

	switch (printMenu(menu_lst, 8))
	{
		case 1:
		{
			menuEngine();
		}
		break;

		case 2:
		{
			menuFan();
		}
		break;

		case 3:
		{
			menuLight();
		}
		break;

		case 4:
		{
			menuTemperature();
		}
		break;

		case 5:
		{
			menuTime();
		}
		break;

/*		case 6:
		{
			menuInfo();
		}
		break;*/

		case 7:
		{
			menuAbout();
		}
		break;

		default:
		{
			state_sys = STATE_WAIT;
			return;
		}
	}
}

void loop(void)
{
	switch (state_sys)
	{
		case STATE_SLEEP:
		{
			stateSleep();
		}
		break;

		case STATE_DRIVE:
		{
			stateDrive();
		}
		break;

		case STATE_WAIT:
		{
			stateWait();
		}
		break;

		case STATE_MENU:
		{
			menuMain();
		}
		break;
	}
}