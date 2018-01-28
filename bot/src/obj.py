#!/usr/bin/env python
# -*- coding: utf-8 -*-
import telebot
from telebot import types
import json
import socket
import urllib
import datetime
from urllib import request
from security import *

def findUserByChatID(chat_id, list_user):
	for user in list_user:
		if (str(chat_id) == str(user.getChatID())):
			return user
	return None

conf_devices = json.loads(open("../conf/devices.json", encoding='utf-8').read())
sha = ClassSHA()

class ClassUser(object):
	def __init__(self, bot, chat_id, name):
		self.__bot = bot
		self.__name = name
		self.__chat_id = chat_id
		self.__list_device = []
		self.__menu_pos = 0

	def addObj(self, obj):
		self.__list_device.append(obj)

	def getChatID(self):
		return self.__chat_id

	def kbrdMain(self):
		kbrd = types.ReplyKeyboardMarkup()
		kbrd.row("My devices", "Add device")
		return kbrd

	def __kbrdGenDeviceS(self):
		kbrd = types.ReplyKeyboardMarkup()
		for device in conf_devices:
			kbrd.row(str(device))
		kbrd.row("Back")
		self.__bot.send_message(self.__chat_id, "Available devices", reply_markup = kbrd)
	
	def __write(self, text, kbrd = None):
		self.__bot.send_message(self.__chat_id, str(text), reply_markup = kbrd)

	def __getDeviceByName(self, name):
		for device in conf_devices:
			if (str(device) == str(name)):
				return conf_devices[device]
		return None

	def __findDeviceByName(self, name):
		for device in self.__list_device:
			if (str(device.getName()) == str(name)):
				return device
		return None

	def __kbrdMyDevices(self):
		kbrd = types.InlineKeyboardMarkup()
		for device in self.__list_device:
			kbrd.add(types.InlineKeyboardButton(text = str(device.getName()), callback_data = "device;" + str(device.getName())))
		return kbrd

	def save(self):
		user_db = json.loads(open("../db/users.json", encoding='utf-8').read())

		if (str(self.__chat_id) not in user_db):
			user_db[str(self.__chat_id)] = dict(name = str(self.__name), devices = "")

		for device in self.__list_device:	
			if (user_db[str(self.__chat_id)]["devices"]):
				device_array = str(user_db[str(self.__chat_id)]["devices"]).split(',')

				if (str(device.getName()) not in device_array):
					user_db[str(self.__chat_id)]["devices"] += "," + str(device.getName())
			else:
				user_db[str(self.__chat_id)]["devices"] = str(device.getName())

		json.dump(user_db, ensure_ascii = False, indent = 4, fp = open("../db/users.json", "w", encoding = 'utf-8'))

	def msgProcess(self, text):
		if (str(text) == "/start"):
			self.__menu_pos = 0;
			self.__write("Main menu", self.kbrdMain())
			return
			
		if (self.__menu_pos == 0):
			if (text == "Add device"):
				self.__menu_pos = 1
				self.__kbrdGenDeviceS();

			elif (text == "My devices"):
				if (len(self.__list_device) == 0):
					self.__write("You not have any devices!")
				else:
					self.__write("Your devices:", self.__kbrdMyDevices())

		elif (self.__menu_pos == 1):
			if (str(text) == "Back"):
				self.__menu_pos = 0;
				self.__write("Main menu", self.kbrdMain())
			else:
				device = self.__getDeviceByName(text);

				if (device == None):
					self.__write("Not found, try again")
				else:
					device2 = self.__findDeviceByName(text);

					if (device2 == None):
						if (device["type"]):
							self.__list_device.append(ClassCar(self.__bot, text, device["id"]))
							self.save()
						self.__write("Added to your devices")
					else:
						self.__write("Already in your devices")

	def callbackProcess(self, call):
		array = str(call.data).split(';')

		if (array[0] == "device"):
			device = self.__findDeviceByName(array[1]);

			# print(call.message.message_id)
			self.__bot.delete_message(chat_id = self.__chat_id, message_id = call.message.message_id)

			if (device == None):
				self.__write("Device not found")
			else:
				device.getControlPanel(self.__chat_id)

		elif (array[0] == "control"):
			device = self.__findDeviceByName(array[1]);

			if (device == None):
				self.__write("Device not found")
			else:
				device.callbackProcess(self.__chat_id, array[2], call.message.message_id)


class ClassObj(object):
	"""docstring for ClassObj"""
	__bot = None

	def __init__(self, bot, name, id):
		self.__id = id
		self.__name = name
		self.__bot = bot

	def getName(self):
		return self.__name

# class ClassCar(ClassObj):
class ClassCar(ClassObj):
	def __init__(self, bot, name, id):
		# super(ClassCar, self).__init__(bot, name)
		self.__id = id
		self.__name = name
		self.__bot = bot
		self.__control_panel_id = None;
		self.__conf = json.loads(open("../conf/conf.json", encoding='utf-8').read())

	def __genPanel(self, chat_id, status):
		kbrd = types.InlineKeyboardMarkup()
		kbrd.add(types.InlineKeyboardButton(text = "Start engine", callback_data = "control;" + str(self.__name) + ";engineStart"), types.InlineKeyboardButton(text = "Stop engine", callback_data = "control;" + str(self.__name) + ";engineStop"))
		kbrd.add(types.InlineKeyboardButton(text = "Get engine timer", callback_data = "control;" + str(self.__name) + ";engineTimer"))
		kbrd.add(types.InlineKeyboardButton(text = "Add number", callback_data = "control;" + str(self.__name) + ";newNumber"), types.InlineKeyboardButton(text = "Add RFID", callback_data = "control;" + str(self.__name) + ";newRFID"), types.InlineKeyboardButton(text = "Get status", callback_data = "control;" + str(self.__name) + ";getStatus"))
		panel_id = self.__bot.send_message(chat_id, "Device: " + str(self.__name) + "\nStatus: " + str(status), reply_markup = kbrd)
		panel_id = panel_id.message_id
		return panel_id

	def getControlPanel(self, chat_id):
		if (self.__control_panel_id != None):
			# Удаление предыдущей контрольной панели
			self.__bot.delete_message(chat_id = chat_id, message_id = self.__control_panel_id)

		self.__control_panel_id = self.__genPanel(chat_id, "Ok")

	def __serverSend(self, data):
		try:
			now = datetime.datetime.now()

			if (int(now.hour) < 10):
				now = "0" + str(now.hour)
			else:
				now = str(now.hour)

			seed = str(self.__conf["server"]["password"]) + str(now)
			password = str(sha.gen(seed))
			request = "http://" + str(self.__conf["server"]["address"]) + ":" + str(self.__conf["server"]["port"]) + "/"
			request += ";ADMIN;" + password + ";" + str(self.__id) + ";" + str(data) + ";"
			doc = urllib.request.urlopen(request, timeout = 30)
			data = str(doc.read()[:350], 'utf-8')
			return data
		except Exception as e:
			return e

	def callbackProcess(self, chat_id, command, msg_id):
		if (self.__control_panel_id == None):
			self.__bot.delete_message(chat_id = chat_id, message_id = msg_id)
			self.__bot.send_message(chat_id = chat_id, text = "Bot has been rebooted, please, select device again")
			return;
			
		if (command == "engineStart"):
			command = "1"
			str_command = "start engine"
		elif (command == "engineStop"):
			command = "2"
			str_command = "stop engine"
		elif (command == "engineTimer"):
			command = "4"
			str_command = "get engine timer"
		else:
			str_command = "get status"
			command = "3"

		self.__bot.edit_message_text(chat_id = chat_id, message_id = self.__control_panel_id, text = "Command \"" + str_command + "\" sended...")

		answer = str(self.__serverSend(command))

		if (answer == "9"):
			str_command += " - \u2705"
		elif (answer == "8"):
			str_command += " - \u274C"
		elif (answer == "NOT_FOUND"):
			str_command = "device not connected"
		elif (answer == "Ok"):
			str_command = "password wrong"
		elif (answer == "timed out"):
			str_command = "device not answered"
		else:
			str_command = "unknown"

		# Статус машины
		if (command == "3") and (answer[0:2] == "0b"):
			str_command = "\n"

			if (answer[2] == '1'):
				str_command += "\u2705 Engine work\n"
			else:
				str_command += "\u274C Engine stopped\n"

			if (answer[3] == '1'):
				str_command += "\u2705 Handbrake up\n"
			else:
				str_command += "\u274C Handbrake down\n"

			if (answer[4] == '1'):
				str_command += "\u26A0 FAIL STATE \u26A0\n"
			else:
				str_command += "\u2705 No fail\n"

		# Таймер двигателя
		if (command == "4") and (len(answer) == 1):
			str_command = ord(answer[0])

		if (self.__control_panel_id != None):
			# Удаление предыдущей контрольной панели
			self.__bot.delete_message(chat_id = chat_id, message_id = self.__control_panel_id)
		self.__control_panel_id = self.__genPanel(chat_id, str_command)

	def getName(self):
		return self.__name