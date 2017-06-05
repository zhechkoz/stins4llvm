import argparse
from tempfile import TemporaryDirectory
from subprocess import run, STDOUT, PIPE
from os import path
import json
from struct import unpack

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

        # show the details of all test cases
        p = run(["for ktest in "+ path.join(tmpdir, "klee-last") +"/*.ktest; do " + KTEST + " --write-ints $ktest; echo ""; done"], stdout=PIPE, shell=True)
        
        out = p.stdout.decode('unicode_escape')
        
        with open(path.join('/tmp', 'klee.json'), 'w') as f:
        	f.write(parse(out))
        # input("Press enter to delete " + tmpdir)

def packerParser(size, parameter):
	result = 0
	try:
		return str(int(parameter))
	except:
		pass
	
	sizeChar = ''
	
	if size == 1:
		sizeChar = 'c'
	elif size == 2:
		sizeChar = 'h'
	elif size == 4:
		sizeChar = 'i'
	elif size == 8:
		sizeChar = 'q'
	
	result = unpack('<' + sizeChar, bytes(parameter, 'ISO-8859-1'))[0]
	
	return str(result)

def parse(kleeOutput):
	output = {}
	kleeOutput = kleeOutput.split('\n\n')[:-1]
	i = 0
	nextIsString = False
	
	for testCase in kleeOutput:
		testCase = testCase.split('\n')[2:]
			
		numObjects = int(testCase[0].split(':')[1])
		
		# Result is not valid
		if numObjects > 0 and testCase[-3].split(':')[2].strip() != "'macke_result'":
			continue
		
		output[str(i)] = {}
		output[str(i)]["parameter"] = []
		
		for j in range(1, numObjects - 1):
			if "macke_sizeof" in testCase[1+j*3].split(':')[2].strip():
				nextIsString = True
				continue
			parameter = testCase[1+j*3+2].split(':')[2].strip()
			parameter = parameter.replace("'", "")
			
			if nextIsString:
				output[str(i)]["parameter"] += [parameter] # Save the string
				nextIsString = False # Parse next
			else:
				size = int(testCase[1+j*3+1].split(':')[2].strip())
				parsed = packerParser(size, parameter)

				output[str(i)]["parameter"] += [parsed]
		
		result = testCase[-1].split(':')[2].strip()
		result = result.replace("'", "")
		
		if nextIsString:
			output[str(i)]["result"] = result
			nextIsString = False
		else:
			size = int(testCase[-2].split(':')[2].strip())
			parsed = packerParser(size, result)
			
			output[str(i)]["result"] = parsed	
		
		
		i += 1	
				
	return json.dumps(output, ensure_ascii=False)
	
	

if __name__ == '__main__':
    # Some argument parsing
    parser = argparse.ArgumentParser(description='Get input for arbitrary functions')
    parser.add_argument('functionname', help='name of the function to generate inputs for')
    parser.add_argument('bcfile', type=argparse.FileType('r'), help='.bc-file containing the compiled bitcode')

    args = parser.parse_args()

    run_KLEE(args.functionname, args.bcfile.name)
    
