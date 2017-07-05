import r2pipe

r2 = r2pipe.open('./InterestingProgram-rewritten')

def patchReport(address):
	# Write an unconditional JMP to
	# jump over report function
	r2.cmd('s ' + address)
	r2.cmd('wa ret')

def findReport(functions):
	killFunctions = []

	# We know report function kills the process
	for f in functions:
		if ('call sym.imp.kill' in r2.cmd('pdf @ ' + f)):
			killFunctions += [f]

	return killFunctions

if __name__ == "__main__":
	r2.cmd('oo+') # Open binary for writing
	r2.cmd('aa') # Analyse functions

	numFunctions = int(r2.cmd('afl~?'))
	nameFunctions = [x['name'] for x in r2.cmdj('aflj') if 'std' not in x['name']]

	reportCandidates = findReport(nameFunctions)

	if len(reportCandidates) <= 0:
		print("ERROR: report function not found!")
		exit(1)

	print('Possible report functions:')
	for f in reportCandidates:
		print('\t' + f)

	print('Pwn only the first found!')
	report = reportCandidates[0]

	patchReport(report)

	r2.quit()
