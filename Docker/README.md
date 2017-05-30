# Introduction to this docker image

Never run KLEE as root, as it can cause serious demage to your operation system (or docker container)

## Important commands

For symbolic test case generation
```
python3 ~/klee/syminputC.py functionname cfile [cfile ...]

python3 ~/klee/syminputBC.py functionname bcfile
```

For playing with the examples
```
make help
```
in the corresponding directories


## Directory structure

*angr* contains all the example files from the lecture to use angr for extracting the license key from the example programs.

*klee* contains two python scripts that wraps KLEE for automatic test case generation.

Unforunatelly, KLEE relies on a very old version of LLVM (3.4 from 2014) so this version must be build manually for this image.
*klee-install* contains an automatic build script for building KLEE with all its dependencies.
The final build is stored in the *build*-directory.
As a consequence building this container on your own might take a while ...
