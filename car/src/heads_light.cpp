#include <avr/io.h>
#include <util/delay.h>

#define INIT_HIGH_LIGHT_SIG	DDRB &= ~(1 << PB3)
#define INIT_DAY_LIGHT_SIG	DDRB &= ~(1 << PB4)
#define INIT_DAY_LIGHT 		DDRB |= (1 << PB0)
#define INIT_HIGH_LIGHT 	DDRB |= (1 << PB1)

#define SET_HIGH_LIGHT_ON	PORTB |= (1 << PB1)
#define SET_HIGH_LIGHT_OFF	PORTB &= ~(1 << PB1)

#define GET_HIGH_LIGHT_SIG	(PINB & (1 << PB3))
#define GET_DAY_LIGHT_SIG	(PINB & (1 << PB4))

#define GET_DAY_LIGHT		OCR0A
#define GET_HIGH_LIGHT		(PORTB & (1 << PB1))

#define SET_DAY_LIGHT_ON(v)	TCCR0B |= (1 << CS01);\
							TCCR0A |= (1 << WGM01) | (1 << WGM00) | (1 << COM0A1);\
							OCR0A = v

#define SET_DAY_LIGHT_OFF	OCR0A = 0;\
							TCCR0B &= ~(1 << CS01);\
							TCCR0A &= ~((1 << WGM01) | (1 << WGM00) | (1 << COM0A1))

uint8_t i = 0;
uint32_t count = 0;

int16_t main()
{
	INIT_DAY_LIGHT;
	INIT_HIGH_LIGHT;
	INIT_DAY_LIGHT_SIG;
	INIT_HIGH_LIGHT_SIG;

	for (;;)
	{
		if ((!GET_DAY_LIGHT_SIG) && (GET_DAY_LIGHT == 0))
		{
			_delay_ms(1000);

			if (!GET_DAY_LIGHT_SIG)
			{
				for (i = 0; i < 255; ++i)
				{
					SET_DAY_LIGHT_ON(i);
					_delay_ms(15);
				}
			}
		}

		if ((GET_DAY_LIGHT_SIG) && (GET_DAY_LIGHT != 0))
		{
			_delay_ms(1000);

			if (GET_DAY_LIGHT_SIG)
			{
				count = 0;

				while (count != 60000)
				{
					count++;
					if (!GET_DAY_LIGHT_SIG)
					{
						break;
					}

					_delay_ms(1);
				}

				if (count != 60000)
				{
					continue;
				}

				i = GET_DAY_LIGHT;

				for (i; i != 0; --i)
				{
					SET_DAY_LIGHT_ON(i);
					_delay_ms(15);
				}

				SET_DAY_LIGHT_OFF;
			}	
		}

		if ((!GET_HIGH_LIGHT_SIG) && (!GET_HIGH_LIGHT))
		{
			_delay_ms(1000);

			if (!GET_HIGH_LIGHT_SIG)
			{
				SET_HIGH_LIGHT_ON;

				if (GET_DAY_LIGHT != 0)
				{
					i = GET_DAY_LIGHT;

					for (i; i != 75; --i)
					{
						SET_DAY_LIGHT_ON(i);
						_delay_ms(15);
					}
				}
			}
		}

		if ((GET_HIGH_LIGHT_SIG) && (GET_HIGH_LIGHT))
		{
			_delay_ms(1000);

			if (GET_HIGH_LIGHT_SIG)
			{
				SET_HIGH_LIGHT_OFF;

				if (GET_DAY_LIGHT != 0)
				{
					i = GET_DAY_LIGHT;

					for (i; i < 255; ++i)
					{
						SET_DAY_LIGHT_ON(i);
						_delay_ms(15);
					}		
				}
			}
		}
	}
}