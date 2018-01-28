import hashlib

class ClassSHA(object):
	def __init__(self):
		pass

	def gen(self, str):
		hash_sum = hashlib.sha256(bytes(str, 'utf-8'))
		return hash_sum.hexdigest()
