import angr
import argparse
import time

# Some basic argument parsing
parser = argparse.ArgumentParser()
parser.add_argument("-v", "--verbose", help="print all resulting states",
                    action="store_true")
parser.add_argument("binary", help="binary to be analysed")

args = parser.parse_args()

start_time = time.clock()

# Read the binary in
p = angr.Project(args.binary)

# Prepare an symbolic argument with 16 characters
arg1 = angr.claripy.BVS('arg1', 16 * 8)

# Create an entry state using this argument
entry = p.factory.entry_state(args=[args.binary, arg1])

# Let symbolic execution do its magic
pg = p.factory.path_group(entry)

if args.verbose:
	# Just fully explore everything
	pg.explore()

	# And show the results
	for path in pg.deadended:
		print "->", repr(path.state.se.any_str(arg1))
else:
	# This time just look only for the relevant path
	pg.explore(find=lambda path: 'license is valid' in path.state.posix.dumps(1))

	# And show only the relevant paths
	for path in pg.found:
		print "->", repr(path.state.se.any_str(arg1))

print "Total runtime:", time.clock() - start_time, "seconds"
