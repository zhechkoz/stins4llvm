import argparse
from tempfile import TemporaryDirectory
from subprocess import run, STDOUT, PIPE
from os import path
import json

# small config section
OPT  = path.expanduser("~/build/llvm/Release/bin/opt")
MACKEOPT = path.expanduser("~/build/macke-opt-llvm/bin/libMackeOpt.so")
KLEE = path.expanduser("~/build/klee/Release+Asserts/bin/klee")
KTEST = path.expanduser("~/build/klee/Release+Asserts/bin/ktest-tool")


def run_KLEE(functionname, bcfile):
    # Put all KLEE generated files into a temporary directory
    with TemporaryDirectory() as tmpdir:
        run([OPT, "-load", MACKEOPT,
             "-encapsulatesymbolic", "--encapsulatedfunction=" + functionname,
             bcfile, "-o", path.join(tmpdir, "prepared.bc")], stdout=PIPE, stderr=PIPE, check=True)
        run([KLEE,
             "--entry-point=macke_" + functionname + "_main", "--max-time=300", "-watchdog",
             "--posix-runtime", "--libc=uclibc",
             "--only-output-states-covering-new", path.join(tmpdir, "prepared.bc")
            ], stdout=PIPE, stderr=PIPE, check=True)
        print()

        # show the details of all test cases
        p = run(["for ktest in "+ path.join(tmpdir, "klee-last") +"/*.ktest; do " + KTEST + " --write-ints $ktest; echo ""; done"], stdout=PIPE, shell=True)
        
        out = p.stdout.decode('utf-8')
        print(parse(out))
        
        # input("Press enter to delete " + tmpdir)

def parse(kleeOutput):
	output = {}
	kleeOutput = kleeOutput.split('\n\n')[:-1]
	i = 0
	
	for testCase in kleeOutput:
		testCase = testCase.split('\n')[2:]
			
		numObjects = int(testCase[0].split(':')[1])
		
		if numObjects > 0 and testCase[-3].split(':')[2].strip() != "'macke_result'":
			continue
		
		output[str(i)] = {}
		output[str(i)]["parameter"] = []
		for j in range(1, numObjects - 1):
			if "macke_sizeof" in testCase[1+j*3].split(':')[2].strip():
				continue
			parameter = testCase[1+j*3+2].split(':')[2].strip()
			parameter = parameter.replace("'", "")
				
			output[str(i)]["parameter"] += [parameter]  
		
		result = testCase[-1].split(':')[2].strip()
		result = result.replace("'", "")
					
		output[str(i)]["result"] = result
		
		i += 1					
	return json.dumps(output)
	
	

if __name__ == '__main__':
    # Some argument parsing
    parser = argparse.ArgumentParser(description='Get input for arbitrary functions')
    parser.add_argument('functionname', help='name of the function to generate inputs for')
    parser.add_argument('bcfile', type=argparse.FileType('r'), help='.bc-file containing the compiled bitcode')

    args = parser.parse_args()

    run_KLEE(args.functionname, args.bcfile.name)
    
