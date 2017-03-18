from cffi import FFI
ffi = FFI()

ffi.cdef('int kvmc_cmd_version(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_help(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_stat(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_stop(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_pause(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_resume(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_balloon(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_debug(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_list(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_start(int argc, const char **argv, const char *prefix);')
ffi.cdef('int kvmc_cmd_setup(int argc, const char **argv, const char *prefix);')

libkvmc = ffi.verify("#include <kvmc.h>", include_dirs=['.'], library_dirs=['.'], libraries=['kvmc'])

def gen_args(*args):
	argc = 0
	argv_list = []
	for value in args:
		argv_list.append(ffi.new('char[]', bytes(value, encoding='utf8')))
		argc += 1
	argv = ffi.new('char *[]', argv_list)
	return argc, argv

def version():
	libkvmc.kvmc_cmd_version(0, ffi.NULL, ffi.NULL)

def help(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_help(argc, argv, ffi.NULL)

def stat(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_stat(argc, argv, ffi.NULL)

def stop(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_stop(argc, argv, ffi.NULL)

def pause(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_pause(argc, argv, ffi.NULL)

def resume(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_resume(argc, argv, ffi.NULL)

def balloon(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_balloon(argc, argv, ffi.NULL)

def debug(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_debug(argc, argv, ffi.NULL)

def list(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_list(argc, argv, ffi.NULL)

def start(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_start(argc, argv, ffi.NULL)

def setup(*args):
	argc, argv = gen_args(args)
	libkvmc.kvmc_cmd_setup(argc, argv, ffi.NULL)

