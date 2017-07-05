import r2pipe
from sys import argv

if len(argv) != 2:
	print('Please provide program name!')
	exit(1)

r2 = r2pipe.open(argv[1])

def seek(addr):
	r2.cmd('s '+ str(addr))

def currentInst(instruction):
	if not instruction or len(instruction) == 0:
		return (None, None, None)

	# Take the first instruction
	currentInstruction = instruction[0]

	type = None
	opcode = None
	size = None

	# Populate instruction parameters
	if currentInstruction.get('type'):
		type = currentInstruction['type']
	if currentInstruction.get('opcode'):
		opcode = currentInstruction['opcode']
	if currentInstruction.get('size'):
		size = int(currentInstruction['size'])

	return (type, opcode, size)

def findSubInstr(start, size):
	address = start

	while address < start + size:
		seek(address)
		instruction = r2.cmdj('pdj 1')
		(type, opcode, instSize) = currentInst(instruction)

		# If the instruction is a sub
		# convert it it add
		if type == 'sub':
			yield address

		address += instSize

def patchSub(address):
	# Convert sub to add by using the same
	# instruction kind but different operator
	seek(address + 1)
	r2.cmd('wx 0x03')

if __name__ == "__main__":
	r2.cmd('oo+') # Open binary for writing
	r2.cmd('aa') # Analyse functions

	subAddr = int(r2.cmd('?v sym.sub'), 16)

	if (subAddr == 0x0):
		print('sub function cannot be found!')
		exit(1)

	size = r2.cmdj('afij @ sym.sub')[0]['size']

	for subs in findSubInstr(subAddr, size):
		patchSub(subs)

	r2.quit()
