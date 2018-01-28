#!/usr/bin/env python
# -*- coding: utf-8 -*-

import telebot
import json
from obj import *
from security import *

list_user = []

sha = ClassSHA()
conf = json.loads(open("../conf/conf.json", encoding='utf-8').read())
bot = telebot.TeleBot(conf["bot"]["token"])

@bot.message_handler()
def messageHandler(message):
	user = findUserByChatID(message.chat.id, list_user)

	if (user == None):
		if (str(sha.gen(message.text)) == conf["bot"]["password"]):
			list_user.append(ClassUser(bot, message.chat.id, str(message.chat.last_name) + str(message.chat.first_name)))
			user = findUserByChatID(message.chat.id, list_user)
			bot.send_message(message.chat.id, "Password correct", reply_markup = user.kbrdMain())
		else:
			bot.send_message(message.chat.id, "Input password")
	else:
		user.msgProcess(message.text)

@bot.callback_query_handler(func=lambda call: True)
def callbackHandler(call):
	user = findUserByChatID(call.message.chat.id, list_user)

	if (user == None):
		bot.delete_message(chat_id = call.message.chat.id, message_id = call.message.message_id)
	else:
		user.callbackProcess(call)

if __name__ == '__main__':
	user_db = json.loads(open("../db/users.json", encoding='utf-8').read())
	devices_db = json.loads(open("../conf/devices.json", encoding='utf-8').read())

	for user in  user_db:
		list_user.append(ClassUser(bot, str(user), str(user_db[user]["name"])))
		user_local = findUserByChatID(str(user), list_user)
		device_array = str(user_db[user]["devices"]).split(',')

		for device in device_array:
			if (str(device) in devices_db):
				if (devices_db[device]["type"] == "car"):
					user_local.addObj(ClassCar(bot, str(device), devices_db[device]["id"]))

	try:
		bot.polling(none_stop=True, timeout=30)
	except Exception:
		print("Error")
		bot.polling(none_stop=True)