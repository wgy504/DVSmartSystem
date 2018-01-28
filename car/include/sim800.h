/*
|	Представленнный ниже код не является свободным
|	и предоставляется только для примера.
|
|	:DVcompany (dima201246@gmail.com)
|	2018
*/

#ifndef SIM800_H_
#define SIM800_H_
	#include "fast_op.h"
	#include "srv_conf.h"

	//	byte_sim800_tmp
	#define SIM_TMP_FOUND_END				0
	#define SIM_TMP_BUF_CLEAR				1
	#define SIM_TMP_BUF_CLEAR_ALL			2
	#define SIM_TMP_ERROR					3
	#define SIM_TMP_COMMAND_SENDED			4
	/*#define SIM_TMP_SMS_AVAILABLE			5*/
	#define SIM_TMP_SRV_DATA_AVAILABLE		6
	#define SIM_TMP_CONNECT_OK				7

	// SIM800 states
	#define SIM_STATE_AT					0
	#define SIM_STATE_SET_FORMAT_MESSAGES	1
	#define SIM_STATE_TEST_CARD				2
	#define SIM_STATE_ERASE_SMS				3
	#define SIM_STATE_NETWORK				4
	#define SIM_STATE_GPRS					5
	#define SIM_STATE_RESET_CONNECTIONS		6
	#define SIM_STATE_SET_APN				7
	#define SIM_STATE_GPRS_CONNECTION		8
	#define SIM_STATE_GET_IP				9
	#define SIM_STATE_CONNECT_SERVER		10
	#define SIM_STATE_READY					11
	#define SIM_SRV_STATE_PREPARE_TO_SEND	12
	#define SIM_SRV_STATE_SEND_DATA			13
/*	#define SIM_SMS_STATE_REQUEST			14
	#define SIM_SMS_STATE_READING_INFO		15
	#define SIM_SMS_STATE_READING_SMS		16
	#define SIM_SMS_STATE_DElETING_SMS		17*/

	uint64_t sim_time_wait_answer = 0;
	uint64_t sim_time_repeat = 0;
	uint64_t sim_time_local = 0;
	uint64_t srv_time_keep_alive = 0;
	volatile uint8_t byte_sim800_tmp = 0;

	// Для UART
	volatile uint8_t sim_buf[200] = {};
	volatile uint8_t sim_buf_len = 0;

	// For clear buf, NOT USE!!!
	volatile uint8_t sim_i, sim_j, sim_k;

	// Для получения одного байта с UART
	volatile uint8_t sim_buf_char = 0;

	// Для получения результата отправки команды
	uint8_t answer= 0;

	// Массив для отправки на сервер
	uint8_t srv_data[7] = {'D', 'V', 'S', 'C', '1', '0', '\0'};
	uint8_t srv_cache_command = 0;
	uint8_t srv_data_wait = 0;

	// Массив для приёма команды
	volatile uint64_t srv_time_command = 0;
	volatile uint8_t srv_prevision_command = 0;
	volatile uint8_t srv_buf[12] = {};
	volatile uint8_t srv_buf_length = 0;

	// Состояние модема
	uint8_t sim_state = 0;

	// Для обработчика SMS
	uint8_t sms_sender_number[12] = {};
	uint8_t sms_message[50] = {};
	uint8_t sms_message_length = 0;
	uint8_t sms_num[2] = {};
	uint8_t sms_num_length = 0;

	// Answers
	#define SIM_NOT_OK		0
	#define SIM_OK			1

	#define SIM800_bufClear()		bitSet(byte_sim800_tmp, SIM_TMP_BUF_CLEAR)
	#define SIM800_bufClearAll()	bitSet(byte_sim800_tmp, SIM_TMP_BUF_CLEAR_ALL)

	uint8_t SIM800_search(uint8_t* source, uint8_t source_len, const char* text);
	uint8_t SIM800_searchUnread();
	uint8_t SIM800_available();

	// SIM800_sendCommand returns
	#define SIM_COM_FAIL		0
	#define SIM_COM_RESULT_OK	1
	#define SIM_COM_SENDED		2
	#define SIM_COM_WAIT_ANSWER	3

	void SIM800_serverSend(uint8_t command)
	{
		srv_data[5] = command;
		sim_state = SIM_SRV_STATE_PREPARE_TO_SEND;
	}

	uint8_t SIM800_sendCommand(const char* command, const char* waited_answer, uint32_t local_time_wait, uint32_t local_time_repeat)
	{
		if (bitGet(byte_sim800_tmp, SIM_TMP_COMMAND_SENDED))
		{
			if ((local_time_wait > 0) && (millis() - sim_time_wait_answer) >= local_time_wait)
			{
				bitUnSet(byte_sim800_tmp, SIM_TMP_COMMAND_SENDED);
				return SIM_COM_FAIL;
			}

			if ((local_time_repeat > 0) && ((millis() - sim_time_repeat) >= local_time_repeat))
			{
				SIM800_bufClearAll();
				for (uint8_t i = 0; command[i] != '\0'; ++i)
				{
					Serial.print(command[i]);
				}
				Serial.println();
				sim_time_repeat = millis();
			}

			if (SIM800_available())
			{
				if (SIM800_search((uint8_t *)sim_buf, sim_buf_len, waited_answer))
				{
					SIM800_bufClearAll();
					bitUnSet(byte_sim800_tmp, SIM_TMP_COMMAND_SENDED);
					return SIM_COM_RESULT_OK;
				}
				else
				{
					SIM800_bufClear();
				}
			}

			return SIM_COM_WAIT_ANSWER;
		}
		else
		{
			for (uint8_t i = 0; command[i] != '\0'; ++i)
			{
				Serial.print(command[i]);
			}
			Serial.println();

			sim_time_repeat = millis();
			sim_time_wait_answer = millis();

			bitSet(byte_sim800_tmp, SIM_TMP_COMMAND_SENDED);

			return SIM_COM_SENDED;
		}

		return SIM_COM_FAIL;
	}

	void manual_bufClear()
	{
		for (sim_i = 0; sim_i < sim_buf_len; ++sim_i)
		{
			if ((sim_buf[sim_i] == 10) || (sim_buf[sim_i] == 13))
			{
				if (((sim_i + 1) < sim_buf_len) && ((sim_buf[sim_i + 1] == 10) || (sim_buf[sim_i + 1] == 13)))
				{
					sim_i++;
				}

				sim_j = 0;
				sim_k = sim_i;

				for (sim_i += 1; sim_i < sim_buf_len; ++sim_i)
				{
					sim_buf[sim_j] = sim_buf[sim_i];
					sim_j++;
				}

				sim_buf_len -= sim_k + 1;

				break;
			}
		}
	}

#ifdef DEBUG
	void printBuf()
	{
		cli();
		debug.print("\"");
		for (uint8_t i = 0; i < sim_buf_len; ++i)
		{
			if ((sim_buf[i] == '\r') || (sim_buf[i] == '\n'))
			{
				debug.print("_");
			}
			else
				debug.print((char)sim_buf[i]);
		}
		debug.print("\"");
		debug.print(" | ");
		debug.println(sim_buf_len);
		sei();
	}
#endif

	void SIM800_sendCustom(const char* value, uint8_t value_len, const uint8_t* value2, uint8_t value2_len)
	{
		uint8_t temp_buf[15];

		strcp(value, temp_buf);

		for (uint8_t i = 0; i < value2_len; ++i)
		{
			temp_buf[i + value_len] = value2[i];
		}

		temp_buf[sms_num_length + value_len] = '\n';
		temp_buf[sms_num_length + value_len + 1] = '\r';

		SIM800_bufClearAll();
		Serial.write(temp_buf, (sms_num_length + value_len + 2));
	}

	void SIM800_state()
	{
		if (bitGet(byte_sim800_tmp, SIM_TMP_ERROR))
		{
			sim_buf_len = 0;
			byte_sim800_tmp = 0;

#ifdef DEBUG
debug.println("Fail");
#endif
			sim_state = SIM_STATE_AT;
		}

		switch (sim_state)
		{
			// Состояние ожидания ответа на команду "AT"
			case SIM_STATE_AT:
			{
				answer = SIM800_sendCommand("AT\0", "OK\0", 20000, 2000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Modem found - OK");
#endif
					sim_state = SIM_STATE_SET_FORMAT_MESSAGES;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Установка начальных настроек модема
			case SIM_STATE_SET_FORMAT_MESSAGES:
			{
				answer = SIM800_sendCommand("AT+CMGF=1\0", "OK\0", 5000, 1000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Set format answer - OK");
#endif
					sim_state = SIM_STATE_TEST_CARD;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Тестирование наличия SIM-карты и отсутствие PIN-кода на ней
			case SIM_STATE_TEST_CARD:
			{
				answer = SIM800_sendCommand("AT+CPIN?\0", "+CPIN: READY\0", 5000, 0);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("SIM-card - OK");
#endif
					sim_state = SIM_STATE_NETWORK;
					// sim_state = SIM_STATE_ERASE_SMS;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			case SIM_STATE_ERASE_SMS:
			{
				answer = SIM800_sendCommand("AT+CMGDA=\"DEL ALL\"\0", "OK\0", 5000, 2000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Clear SMS - OK");
#endif
					sim_state = SIM_STATE_NETWORK;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Ожидание подключения к сети
			case SIM_STATE_NETWORK:
			{
				answer = SIM800_sendCommand("AT+CREG?\0", "+CREG: 0,1\0", 30000, 2000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Network - OK");
#endif
					sim_state = SIM_STATE_GPRS;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Ожидание подключения к GPRS
			case SIM_STATE_GPRS:
			{
				answer = SIM800_sendCommand("AT+CGATT?\0", "+CGATT: 1\0", 30000, 2000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("GPRS - OK");
#endif
#ifdef NO_INTERNET
					sim_state = SIM_STATE_READY;
#else
					sim_state = SIM_STATE_RESET_CONNECTIONS;
#endif
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Сброс всех соединений
			case SIM_STATE_RESET_CONNECTIONS:
			{
				answer = SIM800_sendCommand("AT+CIPSHUT\0", "SHUT OK\0", 10000, 1000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Reset connections - OK");
#endif
					sim_state = SIM_STATE_SET_APN;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}

			}
			break;

			// Отправка APN
			case SIM_STATE_SET_APN:
			{
				answer = SIM800_sendCommand("AT+CSTT=\"internet.tele2.ru\",\"\",\"\"\0", "OK\0", 10000, 0);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Set APN - OK");
#endif
					sim_state = SIM_STATE_GPRS_CONNECTION;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Подключение по GPRS
			case SIM_STATE_GPRS_CONNECTION:
			{
				answer = SIM800_sendCommand("AT+CIICR\0", "OK\0", 30000, 2000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("GPRS connection - OK");
#endif
					sim_state = SIM_STATE_GET_IP;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Получение IP-адреса
			case SIM_STATE_GET_IP:
			{
				answer = SIM800_sendCommand("AT+CIFSR\0", ".\0", 30000, 2000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Get IP - OK");
#endif
					sim_state = SIM_STATE_CONNECT_SERVER;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			// Подключение к серверу
			case SIM_STATE_CONNECT_SERVER:
			{
				answer = SIM800_sendCommand(SRV_ADDRESS, "CONNECT OK\0", 60000, 10000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Server connect - OK");
#endif
					bitSet(byte_sim800_tmp, SIM_TMP_CONNECT_OK);
					srv_data[5] = '0';
					sim_state = SIM_SRV_STATE_PREPARE_TO_SEND;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			case SIM_SRV_STATE_PREPARE_TO_SEND:
			{
				answer = SIM800_sendCommand("AT+CIPSEND=6\0", ">\0", 10000, 3000);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Server prepared to getting data...");
#endif

					sim_state = SIM_SRV_STATE_SEND_DATA;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;

			case SIM_SRV_STATE_SEND_DATA:
			{
				answer = SIM800_sendCommand((const char*)srv_data, "SEND OK\0", 3000, 0);

				if (answer == SIM_COM_RESULT_OK)
				{
#ifdef DEBUG
debug.println("Server getted data");
#endif
					sim_state = SIM_STATE_READY;
				}

				if (answer == SIM_COM_FAIL)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}
			}
			break;
/*
			// Генерация запроса SMS-сообщения с нужным номером
			case SIM_SMS_STATE_REQUEST:
			{
#ifdef DEBUG
debug.println("SMS state: reqest SMS...");
#endif
				SIM800_sendCustom("AT+CMGR=\0", 8, sms_num, sms_num_length);

				sim_state = SIM_SMS_STATE_READING_INFO;
				sim_time_local = millis();
				sim_time_repeat = millis();
			}
			break;

			// Ожидание сообщения
			case SIM_SMS_STATE_READING_INFO:
			{
				if ((millis() - sim_time_local) >= 60000)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}

				if ((millis() - sim_time_repeat) >= 10000)
				{
					SIM800_sendCustom("AT+CMGR=\0", 8, sms_num, sms_num_length);
					sim_time_repeat = millis();
				}

				if (SIM800_available())
				{
					if (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "+CMGR:\0") == SIM_OK)
					{
						// Получение номера отправителя
						uint8_t j = 0;

						for (uint8_t i = 2; i < sim_buf_len; ++i)
						{
							if ((sim_buf[i - 2] == ',') && (sim_buf[i - 1] == '\"') &&(sim_buf[i] == '+'))
							{
								for (uint8_t j = 0; j < 12; ++j)
								{
									sms_sender_number[j] = sim_buf[i + j];
								}
								break;
							}
						}

#ifdef DEBUG
debug.print("Sender number: ");
for (uint8_t i = 0; i < 12; ++i)
{
debug.print((char)sms_sender_number[i]);
}
debug.println();
#endif
						// Очистка буфера до самого SMS-сообщения
						while (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "+CMGR:\0") == SIM_OK)
						{
							manual_bufClear();
						}

						sim_state = SIM_SMS_STATE_READING_SMS;
					}
				}
			}
			break;

			// Чтение SMS-сообщения
			case SIM_SMS_STATE_READING_SMS:
			{
				if (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "OK\0") == SIM_OK)
				{
					sms_message_length = 0;

					// Чтение SMS-сообщения до знака \r или \n
					while ((sim_buf[sms_message_length] != '\n') && (sim_buf[sms_message_length] != '\r'))
					{
						sms_message[sms_message_length] = sim_buf[sms_message_length];
						sms_message_length++;

						if (sms_message_length > 50)
						{
							break;
						}
					}
					SIM800_bufClearAll();

#ifdef DEBUG
debug.print("SMS: ");
for (uint8_t i = 0; i < sms_message_length; ++i)
{
debug.print((char)sms_message[i]);
}
debug.println();
#endif
					bitSet(byte_sim800_tmp, SIM_TMP_SMS_AVAILABLE);

					// Удаление SMS
					SIM800_sendCustom("AT+CMGD=\0", 8, sms_num, sms_num_length);

					sim_state = SIM_SMS_STATE_DElETING_SMS;
					sim_time_local = millis();
				}
			}
			break;

			// Удаление прочтённого SMS-сообщения (чтобы не захламляло память)
			case SIM_SMS_STATE_DElETING_SMS:
			{
				if ((millis() - sim_time_local) >= 5000)
				{
					bitSet(byte_sim800_tmp, SIM_TMP_ERROR);
				}

				if (SIM800_available())
				{
					if (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "OK\0") == SIM_OK)
					{
						SIM800_bufClearAll();

#ifdef DEBUG
debug.println("SMS state: deleting SMS - OK");
#endif
						sim_state = SIM_STATE_READY;

						return;
					}
				}
			}
			break;
*/
			case SIM_STATE_READY:
			{
/*				if (((millis() - sim_time_local) >= 5000) && (!bitGet(byte_sim800_tmp, SIM_TMP_SMS_AVAILABLE)))
				{
					Serial.println("AT+CMGL=\"ALL\"");

					sim_time_local = millis();
					while (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "OK\0") != SIM_OK)
					{
						if ((millis() - sim_time_local) >= 5000)
						{
							break;
						}
					}

					if (SIM800_searchUnread() == SIM_OK)
					{
#ifdef DEBUG
debug.println("Found new SMS...");
#endif
						sim_state = SIM_SMS_STATE_REQUEST;
					}

					sim_time_local = millis();
				}*/

#ifndef NO_INTERNET
				if ((millis() - srv_time_keep_alive) >= 10000)
				{
#ifdef DEBUG
debug.println("Send KEEP_ALIVE");
#endif
					SIM800_serverSend('0');
					srv_time_keep_alive = millis();
					return;
				}
#endif

				if (SIM800_available())
				{
					// Если было прислано SMS-сообщение
/*					if (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "+CMTI:\0") && (!bitGet(byte_sim800_tmp, SIM_TMP_SMS_AVAILABLE)))
					{
#ifdef DEBUG
debug.println("SMS getted!");
#endif
						sms_num_length = 0;

						// Получение номера SMS-сообщения
						for (uint8_t i = 0; i < sim_buf_len; ++i)
						{
							if (sim_buf[i] == ',')
							{
								sms_num_length = 1;
								continue;
							}

							if ((sms_num_length) && ((sim_buf[i] < 48) || (sim_buf[i] > 57)))
							{
								sms_num_length--;
								break;
							}

							if (sms_num_length)
							{
								sms_num[sms_num_length - 1] = sim_buf[i];
								sms_num_length++;
							}
						}

						SIM800_bufClearAll();

						sim_state = SIM_SMS_STATE_REQUEST;
					}
					else*/
					{
#ifdef DEBUG
debug.println("Clear:");
printBuf();
#endif
						SIM800_bufClearAll();
					}
				}

				if (srv_data_wait != 0)
				{
					SIM800_serverSend(srv_cache_command);
					srv_data_wait = 0;
				}
			}
			break;
		}
	}

	// Поиск значения в буфере UART
	uint8_t SIM800_search(uint8_t* source, uint8_t source_len, const char* text)
	{
		uint8_t flag;
		uint8_t i, j;
		uint8_t text_lenght = 0;

		//Определяем длину искомой текстовой строки
		for (i = 0; i <= source_len; i++) {
			if (text[i] == '\0'){
				text_lenght = i;
				break;
			}
		}

		//Искомая строка не содержит признака окончания или меньше размера буфера в котором ищем
		if (text_lenght == 0)
			return SIM_NOT_OK;

		for (i = 0; i < source_len; i++)
		{
			if ((source[i] == text[0]) && ((source_len - i) >= text_lenght))
			{
				flag = 1;
				for (j = 0; j < text_lenght; j++)
				{
					if (source[i + j] != text[j])
					{
						flag = 0;
						break;
					}
				}

				if (flag == 1)
				{
					return SIM_OK;
				}
			}
		}

		return SIM_NOT_OK;
	}

	uint8_t SIM800_searchUnread()
	{
		uint8_t sms_buf_info[100];
		uint8_t sms_buf_info_len = 0;

		for (uint8_t i = 0; i < sim_buf_len; ++i)
		{
			if ((sim_buf[i] != '\r') && (sim_buf[i] != '\n'))
			{
				sms_buf_info[sms_buf_info_len] = sim_buf[i];
				sms_buf_info_len++;
			}
			else
			{
				if (SIM800_search(sms_buf_info, sms_buf_info_len, "+CMGL:\0") == SIM_OK)
				{
					if (SIM800_search(sms_buf_info, sms_buf_info_len, "REC UNREAD\0") == SIM_OK)
					{
						for (uint8_t j = 6; j < sms_buf_info_len; ++j)
						{
							// Обрабатывать только цифры
							if ((sms_buf_info[j] >= 48) && (sms_buf_info[j] <= 57))
							{
								sms_num[sms_num_length] = sms_buf_info[j];
								sms_num_length++;
							}

							if (sms_buf_info[j] == ',')
							{
								return SIM_OK;
								break;
							}
						}

						break;
					}
				}

				sms_buf_info_len = 0;
			}
		}

		return SIM_NOT_OK;
	}

	uint8_t SIM800_available()
	{
		if (bitGet(byte_sim800_tmp, SIM_TMP_FOUND_END))
		{
			bitUnSet(byte_sim800_tmp, SIM_TMP_FOUND_END);
			return 1;
		}

		return 0;
	}

	void SIM800_read()
	{
		if (bitGet(byte_sim800_tmp, SIM_TMP_BUF_CLEAR_ALL))
		{
			bitUnSet(byte_sim800_tmp, SIM_TMP_BUF_CLEAR_ALL);
			sim_buf_len = 0;
		}

		while (Serial.available())
		{
			sim_buf_char = Serial.read();

			if ((sim_buf_char != 13) && (sim_buf_char != 0))
			{
				sim_buf[sim_buf_len] = sim_buf_char;

				if ((sim_buf[sim_buf_len] == 10) || (sim_buf_char == '>'))
				{
					bitSet(byte_sim800_tmp, SIM_TMP_FOUND_END);
				}

				sim_buf_len++;
			}
		}

		if ((bitGet(byte_sim800_tmp, SIM_TMP_CONNECT_OK)) && (bitGet(byte_sim800_tmp, SIM_TMP_FOUND_END)) && (!bitGet(byte_sim800_tmp, SIM_TMP_SRV_DATA_AVAILABLE)))
		{
			if (SIM800_search((uint8_t*)sim_buf, sim_buf_len, "SRV:\0") == SIM_OK)
			{
				srv_buf_length = 0;

				for (uint8_t i = 0; i < sim_buf_len; ++i)
				{
					if (sim_buf[i] == ':')
					{
						// Если послана такая же команда меньше, чем через секунду, то не принимать её.
						if ((srv_prevision_command == sim_buf[i + 1]) && ((millis() - srv_time_command) < 1000))
						{
							srv_time_command = millis();
							break;
						}

						srv_buf_length = 1;
						continue;
					}

					if (srv_buf_length)
					{
						if (((sim_buf[i] >= 48) && (sim_buf[i] <= 57)) || (sim_buf[i] == 45))
						{
							srv_buf[srv_buf_length - 1] = sim_buf[i];
							srv_buf_length++;
						}
						else
						{
							srv_buf[srv_buf_length] = '\0';
							srv_buf_length--;
							srv_time_command = millis();
							srv_prevision_command = srv_buf[0];
							bitSet(byte_sim800_tmp, SIM_TMP_SRV_DATA_AVAILABLE);
							break;
						}
					}
				}
			}
		}

		// Очистка буфера до знака \r
		if (bitGet(byte_sim800_tmp, SIM_TMP_BUF_CLEAR))
		{
			manual_bufClear();

			bitUnSet(byte_sim800_tmp, SIM_TMP_BUF_CLEAR);
		}
	}
#endif