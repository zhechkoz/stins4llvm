# stins4llvm

## A State Inspection Tool for LLVM 

**StIns4LLVM** is a protection tool based on a result checking technique intended to secure sensitive pure functions. It was developed during the Software Integrity Protection Practical Course, Summer Term 2017 at Technical University Munich. 

The workflow of **StIns4LLVM** consists of three main phases. Firstly, it generates test cases using [_MACKE_](https://github.com/tum-i22/macke-opt-llvm) for every specified sensitive pure function. After that, a randomly constructed network of checkers determines the guards' place in the code. These guards attempt to verify whether a given function has been tampered with and if this is the case they call a response function. **StIns4LLVM** introduces delayed, probabilistic failures in the protected program in case of a tampering attempt. To be more precise, the response function is configured in such a way that in 20% of the times it does nothing and continues the normal execution path of the binary, while in the other 80% it terminates the program after 0 to 9 seconds. Finally, the last phase of the protection tool injects the guards in the corresponding places and compiles the protected program.

### Authors
- [Johannes Zirngibl](https://github.com/johanneszi)
- [Zhechko Zhechev](https://github.com/zhechkoz)

### Getting Started
1. Execute _make_ in the project's top directory to build the tool.
2. Compose a _json_ configuration file according to this example (all keys are compulsory and self-explanatory):
  ```
  {
    "functionsRC" : ["mul", "add", "sub", "isValidLicenseKey", "addChar"],
    "program" : ["src/InterestingProgram.c"],
    "binary" : "InterestingProgram-rewritten",
    "connectivityRC" : 2,
    "syminputC" : "syminput/syminputC.py",
    "syminputBC" : "syminput/syminputBC.py",
    "verbose" : true
}
  ```
  3. Finally, run **StIns4LLVM** which will execute the LLVM Pass on the provided source files, compile and place the resulting executable in the _build_ folder.
  ```
  ./run.sh -f config.json
  ```
  
  ### Requirements 
- [LLVM 3.9](http://releases.llvm.org/download.html#3.9.1)
- [KLEE](https://github.com/tum-i22/klee-install)
- [Passes for LLVM operations from MACKE](https://github.com/tum-i22/macke-opt-llvm)
- [jsoncpp >=1.8](https://github.com/open-source-parsers/jsoncpp)
- [jq](https://stedolan.github.io/jq/)

### We strongly recommend using the provided Dockerfile
