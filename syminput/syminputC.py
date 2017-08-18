import argparse
from subprocess import run, PIPE
from os import path
from syminputBC import run_KLEE

# Small config section
CLANG = path.expanduser("~/build/llvm/Release/bin/clang")
LINKR = path.expanduser("~/build/llvm/Release/bin/llvm-link")

if __name__ == '__main__':

    # Some argument parsing
    parser = argparse.ArgumentParser(description='Get input for arbitrary functions')
    parser.add_argument('cfile', nargs='+', type=argparse.FileType('r'), help='.c-file containing the code')

    args = parser.parse_args()

    # Put the compiler output into a temporary directory
    tmpdir='/tmp'
    bcfiles = []

    # compile each c file seperately
    for cfile in args.cfile:
        bcfile = path.join(tmpdir, path.basename(cfile.name)[:-1] + 'bc')
        run([CLANG, '-c', '-g', '-O0', '-emit-llvm', cfile.name, '-o', bcfile], stdout=PIPE, stderr=PIPE, check=True)
        bcfiles.append(bcfile)

    # and link everything together
    run([LINKR] + bcfiles + ['-o', path.join(tmpdir, 'everything.bc')], stdout=PIPE, stderr=PIPE, check=True)
