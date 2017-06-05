#include "llvm/Pass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APFloat.h"

#include <Python.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/lexical_cast.hpp>

#define WARNING "\033[43m\033[1mWARNING:\033[0m "
#define ERROR "\033[101m\033[1mERROR:\033[0m "

#define MAX_LEN 4096

using namespace llvm;

// Network
struct Vertex { Function *function; };
struct Edge { std::string blah; };

typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, Vertex, Edge> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<Graph>::edge_descriptor edge_t;
typedef Graph::vertex_descriptor VertexDescriptor;
typedef Graph::vertex_iterator vertex_iter;
typedef Graph::adjacency_iterator adj_vertex_iter;

namespace {
	
	struct StateProtectorPass : public ModulePass {
		static char ID;
		std::vector<std::string> functionsToProtect = {"max", "min"};
		const std::string inputProgram = "/home/zhechev/Developer/SIP/phase3/stins4llvm/src/InterestingProgram.c";
		const std::string syminput = "../Docker/klee/syminputC.py";
		bool verbose = true;
		 
		Type *x86_FP80Ty, *FP128Ty, *boolTy, *strPtrTy, *voidTy;
		
		Json::Value pureFunctionsTestCases;
		 
		StateProtectorPass() : ModulePass(ID) {}
		
		bool doInitialization(Module &M) override;
		bool runOnModule(Module &M) override;	
		
		Json::Value generateTestCasesForFunctions(std::vector<Function *> functions, size_t maxFunctionNameLength);
		Json::Value parseFromFile(std::string fileName);
		
		Graph createCheckerNetwork(std::vector<Function *> allFunctions, 
							   std::vector<Function *> pureFunctions, unsigned int connectivity);
		
		Value* createLLVMValue(IRBuilder<> *builder, std::string value, Type *type);
		void insertProtect(Module &M, Function *checker, Function *checkee);
		void insertRandSeed(Module &M);
    };
    
	bool StateProtectorPass::doInitialization(Module &M) {
		LLVMContext &ctx = M.getContext();
		
	    x86_FP80Ty = Type::getX86_FP80Ty(ctx); 
	    FP128Ty = Type::getFP128Ty(ctx);
	    boolTy = Type::getInt1Ty(ctx);
	    strPtrTy = Type::getInt8PtrTy(ctx);
	    voidTy = Type::getVoidTy(ctx);
	    
	    // Get all existing pure functions and the maximal
	    // length of their names
	    std::vector<Function *> pureFunctions;
	    size_t maxFunctionNameLength = 0;
	    
	    for (auto funcName : functionsToProtect) {
	  		Function *F = M.getFunction(funcName);
	  		if (F == nullptr || F->size() <= 0 || F->isDeclaration()) {
	  			errs() << WARNING << "Function " << funcName << " not found and will be skipped!\n";
	  			continue;
	  		}
	  		
	  		maxFunctionNameLength = std::max(maxFunctionNameLength, funcName.length());
	  		
	  		pureFunctions.push_back(F);
	  	}
	   	
	   	// Generate for all valid functions test cases
	    pureFunctionsTestCases = generateTestCasesForFunctions(pureFunctions, maxFunctionNameLength);
	    
	    // Print all tests 
	    if (verbose) {
	    	Json::StreamWriterBuilder wbuilder;
			std::string output = Json::writeString(wbuilder, pureFunctionsTestCases);
	
			errs() << "==== TEST CASES ====\n\n" 
				   << output << "\n";
	    }
	    
		return false;
	}
	
	Json::Value StateProtectorPass::generateTestCasesForFunctions(std::vector<Function *> functions, 
																				   size_t maxFunctionNameLength) {
		Json::Value outputTestCases;
		FILE *file;
    	int argc = 3;
    	std::vector<wchar_t*> argv(argc, nullptr);
    	
    	// Generate input for the python script
    	argv[0] = new wchar_t[syminput.length() + 1];
    	mbstowcs(argv[0], syminput.c_str(), syminput.length() + 1);
    	
    	argv[1] = new wchar_t[maxFunctionNameLength + 1];
    	
    	argv[2] = new wchar_t[inputProgram.length() + 1];
    	mbstowcs(argv[2], inputProgram.c_str(), inputProgram.length() + 1);
    	
    	
    	for (auto function : functions) {
    		std::string functionName = function->getName();
    		
			mbstowcs(argv[1], functionName.c_str(), functionName.length() + 1);
			
			// Invoke python
			Py_SetProgramName(argv[0]);
    		Py_Initialize();
    		PySys_SetArgv(argc, argv.data());
    		
			file = fopen(syminput.c_str(), "r");
			PyRun_SimpleFile(file, syminput.c_str());
			Py_Finalize();
			
			fclose(file);
			
			// TODO: Make path string OS independent
			// Save intermediate results from parsing
			Json::Value currentFunctionTestCases = parseFromFile("/tmp/klee.json");
			outputTestCases[functionName] = currentFunctionTestCases;
		}
		
		// Release buffer
		for (int i = 0; i < argc; i++) {
			delete[] argv[i];
		}
    	
    	return outputTestCases;
	}
	
	Json::Value StateProtectorPass::parseFromFile(std::string fileName) {
		Json::Value root;   
    	Json::Reader reader;
    	
    	std::ifstream file(fileName);
    	bool parsingSuccessful = reader.parse(file, root, false);
    	
    	if (!parsingSuccessful) {
        	errs()  << WARNING << "Failed to parse file. Tests might be incomplete!\n"
          		    << reader.getFormattedErrorMessages() << "\n";
 		}
 		
 		file.close();
 		
 		return root;
	}

	Graph StateProtectorPass::createCheckerNetwork(std::vector<Function *> allFunctions, 
							   std::vector<Function *> pureFunctions, unsigned int connectivity) {
		Graph g;
		std::vector<vertex_t> checkerFunctions, checkeeFunctions;
		
		for (auto function : allFunctions) {
			vertex_t u = boost::add_vertex(g);
			g[u].function = function;
			checkerFunctions.push_back(u);
			if (std::find(pureFunctions.begin(), pureFunctions.end(), function) != pureFunctions.end()) {
				checkeeFunctions.push_back(u);
			}
		}
		
		if (connectivity > checkerFunctions.size() - 1) {
			errs() << WARNING << "The specified connectivity " 
				   << connectivity << " is larger than the number of functions!\n" 
				   << "Connectivity set to: " << checkerFunctions.size() - 1 << "\n";
		} 
		
		// Don't allow checking a function a by function b more than once  
		connectivity = std::min((unsigned int)(checkerFunctions.size() - 1), connectivity);
		
		for (auto checkee : checkeeFunctions) {
			// Connect enough other vertices as checkers to this node to fulfill the connectivity 
			unsigned int out = 0;
			while(out < connectivity) {
				vertex_t checker;
				// checkers are chosen randomly
				int rand_pos;
				while(true) {
					rand_pos = rand() % checkerFunctions.size();
					checker = checkerFunctions[rand_pos];
					if (checkee != checker){
						break;
					}
				}
				
				// If the edge was added increase the counter and repeat until connectivity is reached
				if(boost::add_edge(checker, checkee, g).second) {
					out += 1;
				}
			}
		}
		
		if (verbose) {
			errs() << "=== CHECKER NETWORK ===\n";
			write_graphviz(std::cerr, g); 
		}
		
		return g;
	}
	
	Value* StateProtectorPass::createLLVMValue(IRBuilder<> *builder, std::string value, Type *type) {
		Value *output = nullptr;
		
		if (type->isPointerTy()) {
			Type *content = type->getContainedType(0);
			if (content->isIntegerTy(8)) {			// char* 
				output = builder->CreateGlobalStringPtr(value);
			} else {
				errs() << ERROR << "Type ";
				type->print(errs());
				errs() << " cannot be resolved with the current implementation!\n";
			}
		} else if (type->isIntegerTy()) {
			if (type->isIntegerTy(8)) {
				output = builder->getInt8(value[0]);
			} else if (type->isIntegerTy(16)) {
				short castedShort = boost::lexical_cast<short>(value);
				output = builder->getInt16(castedShort);
			} else if (type->isIntegerTy(32)) {
				int castedInt = boost::lexical_cast<int>(value);
				output = builder->getInt32(castedInt);
			} else if (type->isIntegerTy(64)) {
				long castedInt = boost::lexical_cast<long>(value);
				output = builder->getInt64(castedInt);
			} else if (type->isIntegerTy(128)) {
				long long castedInt = boost::lexical_cast<long long>(value);
				output = builder->getIntN(128, castedInt);
			} else {
				errs() << ERROR << "Type ";
				type->print(errs());
				errs() << " cannot be resolved with the current implementation!\n";
			}
		} else if (type->isFloatingPointTy()) {
			if (type->isFloatTy()) {
				float castedFloat = boost::lexical_cast<float>(value);
				output = ConstantFP::get(builder->getFloatTy(), castedFloat);
			} else if (type->isDoubleTy()) {
				double castedDouble = boost::lexical_cast<double>(value);
				output = ConstantFP::get(builder->getDoubleTy(), castedDouble);
			} else if (type->isX86_FP80Ty()) {
				long double castedDouble = boost::lexical_cast<long double>(value);
				output = ConstantFP::get(x86_FP80Ty, castedDouble);
			} else if (type->isFP128Ty()) {
				long double castedDouble = boost::lexical_cast<long double>(value);
				output = ConstantFP::get(FP128Ty, castedDouble);
			} 
			
			else {
				errs() << ERROR << "Type ";
				type->print(errs());
				errs() << " cannot be resolved with the current implementation!\n";
			}
		}
		
		if (output == nullptr) {
			errs() << ERROR << "No type could be resolved for value:" << value<< "\n";
			exit(1);
		}
		
		return output;
	}
	
	void StateProtectorPass::insertProtect(Module &M, Function *checker, Function *checkee) {
		
		BasicBlock *firstBasicBlock = &checker->getEntryBlock();
		BasicBlock *reportBlock = BasicBlock::Create(M.getContext(), "reportBlock", checker, firstBasicBlock);
		
		std::vector<BasicBlock *> funcNotOnStack;
		for (unsigned int j = 0; j < pureFunctionsTestCases[checkee->getName()].size(); j++) {
			BasicBlock *checkBlock = BasicBlock::Create(M.getContext(), "funcNotOnStack", checker, reportBlock);
			funcNotOnStack.push_back(checkBlock);
		}
		
		BasicBlock *funcOnStackTest = BasicBlock::Create(M.getContext(), "funcOnStack", checker, funcNotOnStack[0]);
		
		std::vector<Value *> args;
		IRBuilder<> builder(funcOnStackTest);
		Constant *stackFunction = M.getOrInsertFunction("checkTrace", 
					FunctionType::get(boolTy, strPtrTy, false));
		
		std::string functionNameString = checkee->getName();
		Value *functionName = builder.CreateGlobalStringPtr(functionNameString);
		
		args.push_back(functionName);
		Value *stackCall = builder.CreateCall(stackFunction, args);
		
		Value *onStack = builder.getInt1(true);
		Value *stackResult = builder.CreateICmpEQ(onStack, stackCall);
		builder.CreateCondBr(stackResult, firstBasicBlock, funcNotOnStack[0]);
		
		

		Value *checkeeCall;
		Value *result;
		Value *checkerResult;
		for (unsigned j = 0; j < pureFunctionsTestCases[checkee->getName()].size(); j++) {
			BasicBlock *checkBlock = funcNotOnStack[j];
			builder.SetInsertPoint(checkBlock);
			args.clear();
		
			int i = 0;
			for (auto &argument : checkee->getArgumentList()) {
				Value *arg = createLLVMValue(&builder, pureFunctionsTestCases[checkee->getName()][std::to_string(j)]["parameter"][i].asString(), argument.getType());
				args.push_back(arg);
				i++;
			}
		
			checkeeCall = builder.CreateCall(checkee, args);
			result = createLLVMValue(&builder, pureFunctionsTestCases[checkee->getName()][std::to_string(j)]["result"].asString(), checkee->getReturnType());
			
		
			if (checkee->getReturnType()->isIntegerTy()) {
				checkerResult = builder.CreateICmpNE(result, checkeeCall);
			} else if (checkee->getReturnType()->isFloatingPointTy()) {
				checkerResult = builder.CreateFCmpONE(result, checkeeCall);
			} else if (checkee->getReturnType()->isPointerTy() &&  
						checkee->getReturnType()->getContainedType(0)->isIntegerTy(8)) { // char *
				Type *argsTypes[2] = {strPtrTy, strPtrTy};
			
				Constant *strcmpFunction = M.getOrInsertFunction("cmpstr", 
						FunctionType::get(boolTy, ArrayRef<Type *>(argsTypes), false));
			
				args.clear();
				args.push_back(result);
				args.push_back(checkeeCall);		
				result = builder.CreateCall(strcmpFunction, args);
				checkerResult = builder.CreateICmpNE(result, builder.getInt1(false));
			}
			
			if (j == pureFunctionsTestCases[checkee->getName()].size()-1){
				builder.CreateCondBr(checkerResult, reportBlock, firstBasicBlock);
			} else{
				builder.CreateCondBr(checkerResult, reportBlock, funcNotOnStack[j+1]);
			}
		}
		builder.SetInsertPoint(reportBlock);
		Constant *reportFunction = M.getOrInsertFunction("report", 
					FunctionType::get(voidTy, false));
		builder.CreateCall(reportFunction);
		builder.CreateBr(firstBasicBlock);
	}
	
	void StateProtectorPass::insertRandSeed(Module &M) {
		Function *F = M.getFunction("main");
  		if (F != nullptr && F->size() > 0 && !F->isDeclaration()) {
  			Instruction *firstInst = &*(F->getEntryBlock().getFirstInsertionPt());
  			
  			IRBuilder<> builder(firstInst);
  			
  			Constant *initRandomFunction = M.getOrInsertFunction("initRandom", 
						FunctionType::get(voidTy, false));
  			builder.CreateCall(initRandomFunction);
  		}
	}
	
	bool StateProtectorPass::runOnModule(Module &M) { 
		std::vector<Function *> pureFunctions;
	  	std::vector<Function *> allFunctions;
	  	  	
		for (auto &F : M) {
			if (F.size() > 0 && !F.isDeclaration()) {
				allFunctions.push_back(&F);
				
				if (std::find(functionsToProtect.begin(), functionsToProtect.end(), F.getName()) 
					!= functionsToProtect.end()) {
					pureFunctions.push_back(&F);
				}
			}	
		}
	  	
	  	Graph g = createCheckerNetwork(allFunctions, pureFunctions, 1);
	  	
	  	std::pair<vertex_iter, vertex_iter> vp;
    
		// For every function from the graph
		for (vp = vertices(g); vp.first != vp.second; ++vp.first) {
			VertexDescriptor v = *vp.first;
			Function *checkerFunction = g[v].function;
			errs() << "Function " << checkerFunction->getName() << " checks: \n"; 
			std::pair<adj_vertex_iter, adj_vertex_iter> adj_vp;
	  		
	  		for (adj_vp = adjacent_vertices(v, g); adj_vp.first != adj_vp.second; ++adj_vp.first) {
				v = *adj_vp.first;
				Function *checkeeFunction = g[v].function;
				errs() << "\t" << checkeeFunction->getName() << "\n"; 
				// Insert checker
				insertProtect(M, checkerFunction, checkeeFunction);
			}
		}
		
		insertRandSeed(M);
		
	  	return true;		
	}
}

char StateProtectorPass::ID = 0;

RegisterPass<StateProtectorPass> X("stateProtect", "State Protector Pass", false, false);

