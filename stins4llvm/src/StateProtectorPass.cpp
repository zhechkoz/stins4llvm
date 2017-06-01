#include "llvm/Pass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <fstream>

#define WARNING "\033[43m\033[1mWARNING:\033[0m"
#define ERROR "\033[101m\033[1mERROR:\033[0m"

using namespace llvm;

namespace {
	typedef std::vector<std::vector<std::string> > VecInVec;
	
	const std::string CHECKFUNC = "check" , REPORTFUNC = "report";
	const std::string USAGE = "Specify file containing new line separated functions to protect.";
	
	//const cl::opt<std::string> FILENAME("ff", cl::desc(USAGE));
	const std::string FILENAME = "../results/funcsToCheck";
	
	struct StateProtectorPass : public ModulePass {
		static char ID;
		Constant *checkFunction, *reportFunction;
		
		// Store the type instances for primitive types.
		Type *ptrTy, *voidTy, *boolTy ; 
		
		std::vector<std::string> functionsToProtect;
		
		StateProtectorPass() : ModulePass(ID) {}
		
		bool doInitialization(Module &M) override;
		bool runOnModule(Module &M) override;
		
		//void dump(VecInVec callGraph, unsigned int tabs);
		std::vector<std::string> parseFunctionToProtect();	
    };
    
    std::vector<std::string> StateProtectorPass::parseFunctionToProtect() {
		std::vector<std::string> functions;
		std::ifstream infile(FILENAME);
		
		std::string line;
		while (std::getline(infile, line)) {
			if (line == "") { continue; } // Skip empty lines
			functions.push_back(line);
		}
	
		// The user has to provide at least one function to protect
		if(functions.size() == 0) {
			errs() << USAGE << "\n";
			exit(1);
		}
			
		return functions;
	}
    
	bool StateProtectorPass::doInitialization(Module &M) {
		// Check if there is a function in the target program that conflicts
		// with the current set of functions
		if (M.getFunction(StringRef(CHECKFUNC)) != nullptr || 
			M.getFunction(StringRef(REPORTFUNC)) != nullptr) {
			errs() << ERROR << " The target program should not contain functions called"
				   << CHECKFUNC << " or " << REPORTFUNC << "\n";
			exit(1);
		}

		functionsToProtect = parseFunctionToProtect();
		
		/* store the type instances for primitive types */
		ptrTy = Type::getInt8PtrTy(M.getContext());
		voidTy = Type::getVoidTy(M.getContext());
		boolTy = Type::getInt1Ty(M.getContext());
		
		Type *argsTypes[2] = {ptrTy, boolTy};
		
		// Define check and report functions
		checkFunction = M.getOrInsertFunction(CHECKFUNC, 
					FunctionType::get(boolTy, ArrayRef<Type *>(argsTypes), false));
		
		reportFunction = M.getOrInsertFunction(REPORTFUNC, 
					FunctionType::get(voidTy, boolTy, false)) ;
				
		return true;
	}

	
	bool StateProtectorPass::runOnModule(Module &M) {    	
	  	
	  	return true;		
	  }
}

char StateProtectorPass::ID = 0;

RegisterPass<StateProtectorPass> X("state", "State Protector Pass", false, false);

