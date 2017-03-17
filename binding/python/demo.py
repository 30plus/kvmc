def print_args(*args):
	argc = 0
	for value in args:
		print(value)
		argc += 1
	print(argc)

if __name__ == '__main__':
	print_args('Xingyou', 'Chen', 99)
