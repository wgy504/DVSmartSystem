#!/usr/bin/env python
import asyncio
import datetime
import json
import requests
from security import *

db_users = []

dv_smartcar_1 = []
sha = ClassSHA()
config = json.loads(open("../conf/server.json").read())

def getWeather():
	try:
		res = requests.get("http://api.openweathermap.org/data/2.5/weather", params={'id': config["weather"]["city_id"], 'units': 'metric', 'lang': 'ru', 'APPID': config["weather"]["appid"]})
		data = res.json()
		return data
	except Exception as e:
		print("Exception (find):", e)

async def handle_tcp_echo(reader, writer):
	global dv_smartcar_1
	now = datetime.datetime.now()
	print("\n[LOG " + str(now) + "] Connection from {}".format(writer.get_extra_info('peername')))

	while True:
		data = await reader.read(100)   # Read data-message
		now = datetime.datetime.now()
		if data:
			message = data.decode()     # Convert to reatable text

			if (message[len(message) - 1] == '\n'):
				message = message[0:-1]     # Remove "\n" in end

			msg_array = message.split(";")  # Split message by spaces

			# Работа с пользователем
			try:
				if (len(msg_array) >= 4):
					if(msg_array[1] == "ADMIN"):
						tm_seed = datetime.datetime.now()

						if (int(tm_seed.hour) < 10):
							tm_seed = "0" + str(tm_seed.hour)
						else:
							tm_seed = str(tm_seed.hour)

						seed = str(config["password"]) + str(tm_seed)
						password = str(sha.gen(seed))

						if (msg_array[2] == password):
							if (writer not in db_users):
								db_users.append(writer)

							command_sended = 0
							print("[LOG " + str(now) + "] Command: ", str(msg_array[4]))
							if (str(msg_array[3]) == "DVSC1"):
								try:
									# Если это запрос о состоянии машины
									if (str(msg_array[4]) == "3"):
										if (len(dv_smartcar_1) >= 1):
											if (len(dv_smartcar_1) == 1):
												dv_smartcar_1.append("get_status")
											else:
												dv_smartcar_1[1] = "get_status"

									dv_smartcar_1[0].write(str("SRV:" + str(msg_array[4]) + "\n\r").encode())
									command_sended = 1
								except Exception as e:
									if (len(dv_smartcar_1) == 0):
										command_sended = 0
									else:
										command_sended = 2
									print("[LOG " + str(now) + "] FAIL in sending command: ", e)

							if (command_sended != 1):
								if (command_sended == 2):
									answer = "HTTP/1.0 200 OK\nServer: dv_smart_server/0.6.31\nContent-Language: en\nContent-Type: text/html; charset=utf-8\nContent-Length: 12\nConnection: close\n\nFAIL_IN_SEND"
								else:
									answer = "HTTP/1.0 200 OK\nServer: dv_smart_server/0.6.31\nContent-Language: en\nContent-Type: text/html; charset=utf-8\nContent-Length: 9\nConnection: close\n\nNOT_FOUND"

								writer.write(answer.encode())
								writer.close()

								i = 0
								for user in db_users:
									if (user == writer):
										print("[LOG " + str(now) + "] Deleting: admin")
										db_users.pop(i)
										break
									i += 1

								print("[LOG " + str(now) + "] Terminating connection: {}\n".format(writer.get_extra_info('peername')))
								break;
						else:
							print("[LOG " + str(now) + "] Bad password")
							answer = "HTTP/1.0 200 OK\nServer: dv_smart_server/0.6.31\nContent-Language: en\nContent-Type: text/html; charset=utf-8\nContent-Length: 2\nConnection: close\n\nOk"
							writer.write(answer.encode())
							writer.close()
							print("[LOG " + str(now) + "] Terminating connection: {}\n".format(writer.get_extra_info('peername')))
							break;

			except Exception as e:
				writer.write(str("INCORRECT_FORMAT").encode())

			# Работа с устройством
			if ((msg_array[0] == "DVSC10") and (len(dv_smartcar_1) <= 1)):
				if ((len(dv_smartcar_1) == 0) or (dv_smartcar_1[0] != writer)):
					print("[LOG " + str(now) + "] Found DV SmartCar1\n")
					dv_smartcar_1 = []
					dv_smartcar_1.append(writer)

			elif ((msg_array[0] == "DVSC11") and (len(dv_smartcar_1) == 1)):
				temp = round(getWeather()['main']['temp'])
				print("[LOG " + str(now) + "] Send weather to DV SmartCar1")
				print("[LOG " + str(now) + "] Weather temperature: ", temp, "\n")
				dv_smartcar_1[0].write(str("SRV:7" + str(temp) + "\n\r").encode())

			elif ("DVSC1" in msg_array[0]):
				answer = "HTTP/1.0 200 OK\nServer: dv_smart_server/0.6.31\nContent-Language: en\nContent-Type: text/html; charset=utf-8\nContent-Length: 1\nConnection: close\n\n"

				for user in db_users:
					value = msg_array[0][5]
					print("[LOG " + str(now) + "] DV SmartCar1 answer chr: " + str(msg_array[0][5]) + " ord: ", ord(value))

					if ((len(dv_smartcar_1) == 2) and (dv_smartcar_1[1] == "get_status")):
						dv_smartcar_1.pop(1)
						value = ord(value)
						answer = "HTTP/1.0 200 OK\nServer: dv_smart_server/0.6.31\nContent-Language: en\nContent-Type: text/html; charset=utf-8\nContent-Length: 10\nConnection: close\n\n0b"
						str_byte = ""

						for i in range(8):
							if (value & (1 << i)):
								str_byte += '1'
							else:
								str_byte += '0'

						answer += str_byte

						print("[LOG " + str(now) + "] DV SmartCar1 send byte: 0b" + str_byte)

					else:
						answer += str(msg_array[0][5])

					user.write(answer.encode())
					user.close()
					print("[LOG " + str(now) + "] Deleting: admin")
					db_users.remove(user)    # Remove admins socket on users array

			await writer.drain()
		else:
			i = 0

			for user in db_users:
				if (user == writer):
					print("[LOG " + str(now) + "] Deleting: admin")
					db_users.pop(i)
					break
				i += 1

			if (writer == dv_smartcar_1[0]):
				print("\n[LOG " + str(now) + "] Terminating connection with DV SmartCar1: {}\n".format(writer.get_extra_info('peername')))
				dv_smartcar_1 = []
			else:
				print("[LOG " + str(now) + "] Terminating connection: {}\n".format(writer.get_extra_info('peername')))
			writer.close()
			break

if __name__ == "__main__":
	serv_address = config["address"]
	serv_port  = config["port"]

	loop = asyncio.get_event_loop()
	loop.run_until_complete(
		asyncio.ensure_future(
			asyncio.start_server(handle_tcp_echo, serv_address, serv_port),
			loop=loop
		)
	)

	loop.run_forever()