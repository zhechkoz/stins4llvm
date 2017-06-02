import argparse
from tempfile import TemporaryDirectory
from subprocess import run, PIPE
from os import path
from syminputBC import run_KLEE

# Small config section
CLANG = path.expanduser("~/build/llvm/Release/bin/clang")
LINKR = path.expanduser("~/build/llvm/Release/bin/llvm-link")

if __name__ == '__main__':

	# Some argument parsing
	parser = argparse.ArgumentParser(description='Get input for arbitrary functions')
	parser.add_argument('functionname', help='name of the function to generate inputs for')
	parser.add_argument('cfile', nargs='+', type=argparse.FileType('r'), help='.c-file containing the code')

	args = parser.parse_args()

	# Put the compiler output into a temporary directory
	with TemporaryDirectory() as tmpdir:
		# print(tmpdir)
		bcfiles = []

		# compile each c file seperately
		for cfile in args.cfile:
			bcfile = path.join(tmpdir, cfile.name[:-1] + 'bc')
			run([CLANG, '-c', '-g', '-O0', '-emit-llvm', cfile.name, '-o', bcfile], stdout=PIPE, stderr=PIPE, check=True)
			bcfiles.append(bcfile)

		# and link everything together
		run([LINKR] + bcfiles + ['-o', path.join(tmpdir, 'everything.bc')], check=True)
		
		# Finally run KLEE on it
		run_KLEE(args.functionname, path.join(tmpdir, "everything.bc"))
		
		# input("Press enter to delete " + tmpdir)
