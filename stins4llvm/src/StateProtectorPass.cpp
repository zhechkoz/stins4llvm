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
		std::vector<std::string> functionsToProtect = {"isValidLicenseKey"};
		const std::string program = "/home/zhechev/Developer/SIP/phase3/stins4llvm/src/InterestingProgram.c";
		const std::string syminput = "../Docker/klee/syminputC.py";
		
		Type *x86_FP80Ty, *FP128Ty;
		
		Json::Value pureFunctionsTestCases;
		 
		StateProtectorPass() : ModulePass(ID) {}
		
		bool doInitialization(Module &M) override;
		bool runOnModule(Module &M) override;	
		
		Json::Value parse(char * input);
		
		Value* createLLVMValue(IRBuilder<> *builder, std::string value, Type *type);
		void insertProtect(Module &M, Function *checker, Function *checkee);
    };
    
	bool StateProtectorPass::doInitialization(Module &M) {
	
	    x86_FP80Ty = Type::getX86_FP80Ty(M.getContext()); 
	    FP128Ty = Type::getFP128Ty(M.getContext());
	    
		FILE *file;
    	int argc;
    	wchar_t **argv = new wchar_t*[3];
    	for (int i = 0; i < 3; i++) {
    		argv[i] = new wchar_t[MAX_LEN];
    	}
		
		for (auto function : functionsToProtect) {
			char *buffer = new char[MAX_LEN];
			int out_pipe[2];
			int saved_stdout;

			saved_stdout = dup(STDOUT_FILENO);  /* save stdout for display later */

			if( pipe(out_pipe) != 0 ) {          /* make a pipe */
				exit(1);
			}

			dup2(out_pipe[1], STDOUT_FILENO);   /* redirect stdout to the pipe */
			close(out_pipe[1]);
			
			/* anything sent to printf should now go down the pipe */
    		argc = 3;
			mbstowcs(argv[0], syminput.c_str(), syminput.length());
			mbstowcs(argv[1], function.c_str(), function.length());
			mbstowcs(argv[2], program.c_str(), program.length()) ;
			
			Py_SetProgramName(argv[0]);
    		Py_Initialize();
    		PySys_SetArgv(argc, argv);
    		
			file = fopen(syminput.c_str(), "r");
			PyRun_SimpleFile(file, syminput.c_str());
			Py_Finalize();
		
		
			fflush(stdout);

			read(out_pipe[0], buffer, MAX_LEN); /* read from pipe into buffer */

			dup2(saved_stdout, STDOUT_FILENO);  /* reconnect stdout for testing */
			//errs() << buffer;
			Json::Value currentFunctionTestCases = parse(buffer);
			pureFunctionsTestCases[function] = currentFunctionTestCases;
		}
		
		return false;
	}
	
	Json::Value StateProtectorPass::parse(char * input) {
		Json::Value root;   
    	Json::Reader reader;
    	bool parsingSuccessful = reader.parse(input, root);
    	if (!parsingSuccessful) {
        	errs()  << "Failed to parse"
          		    << reader.getFormattedErrorMessages() << "\n";
 		}
 		
 		return root;
	}

	Graph createCheckerNetwork(std::vector<Function *> allFunctions, 
							   std::vector<Function *> pureFunctions, int connectivity = 5) {
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
		
		if (connectivity > (int) checkerFunctions.size() - 1) {
			errs() << WARNING << "The specified connectivity " 
				   << connectivity << " is larger than the number of functions!\n" 
				   << "Connectivity set to: " << checkerFunctions.size() - 1 << "\n";
		} 
		
		connectivity = std::min((int)(checkerFunctions.size() - 1), connectivity);
		
		for (auto checkee : checkeeFunctions) {
			// connect enough other vertices as checkers to this node to fulfill the connectivity 
			int out = 0;
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
				
				// if the edge was added increase the counter and repeat until connectivity is reached
				if(boost::add_edge(checker, checkee, g).second) {
					out += 1;
				}
			}
		}
		
		write_graphviz(std::cerr, g); 
		
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
		BasicBlock *funcNotOnStack = BasicBlock::Create(M.getContext(), "funcNotOnStack", checker, reportBlock);	
		BasicBlock *funcOnStackTest = BasicBlock::Create(M.getContext(), "funcOnStackTest", checker, funcNotOnStack);
		
		std::vector<Value *> args;
		IRBuilder<> builder(funcOnStackTest);
		Constant *stackFunction = M.getOrInsertFunction("checkTrace", 
					FunctionType::get(Type::getInt1Ty(M.getContext()), Type::getInt8PtrTy(M.getContext()), false));
		
		std::string functionNameString = checkee->getName();
		Value *functionName = builder.CreateGlobalStringPtr(functionNameString);
		
		args.push_back(functionName);
		Value *stackCall = builder.CreateCall(stackFunction, args);
		
		Value *onStack = builder.getInt1(true);
		Value *stackResult = builder.CreateICmpEQ(onStack, stackCall);
		builder.CreateCondBr(stackResult, firstBasicBlock, funcNotOnStack);
		
		builder.SetInsertPoint(funcNotOnStack);
		
		args.clear();
		int i = 0;
		for (auto &argument : checkee->getArgumentList()) {
			Value *arg = createLLVMValue(&builder, pureFunctionsTestCases[checkee->getName()]["3"]["parameter"][i].asString(), argument.getType());
			args.push_back(arg);
			i++;
		}
		
		Value *checkeeCall = builder.CreateCall(checkee, args);
		Value *result = createLLVMValue(&builder, pureFunctionsTestCases[checkee->getName()]["3"]["result"].asString(), checkee->getReturnType());
		Value *checkerResult;
		
		if (checkee->getReturnType()->isIntegerTy()) {
			checkerResult = builder.CreateICmpNE(result, checkeeCall);
		} else if (checkee->getReturnType()->isFloatingPointTy()) {
			checkerResult = builder.CreateFCmpONE(result, checkeeCall);
		} else if (checkee->getReturnType()->isPointerTy() &&  
					checkee->getReturnType()->getContainedType(0)->isIntegerTy(8)) { // char *
			Type *argsTypes[2] = {Type::getInt8PtrTy(M.getContext()), Type::getInt8PtrTy(M.getContext())};
			
			Constant *strcmpFunction = M.getOrInsertFunction("cmpstr", 
					FunctionType::get(Type::getInt1Ty(M.getContext()), ArrayRef<Type *>(argsTypes), false));
			
			args.clear();
			args.push_back(result);
			args.push_back(checkeeCall);		
			result = builder.CreateCall(strcmpFunction, args);
			checkerResult = builder.CreateICmpNE(result, builder.getInt1(false));
		}
		
		builder.CreateCondBr(checkerResult, reportBlock, firstBasicBlock);
		
		builder.SetInsertPoint(reportBlock);
		Constant *reportFunction = M.getOrInsertFunction("report", 
					FunctionType::get(Type::getVoidTy(M.getContext()), false));
		builder.CreateCall(reportFunction);
		builder.CreateBr(firstBasicBlock);
	}
	
	bool StateProtectorPass::runOnModule(Module &M) { 
	
		Json::StreamWriterBuilder wbuilder;
		std::string output = Json::writeString(wbuilder, pureFunctionsTestCases);
	
		errs() << output;
		std::vector<Function *> pureFunctions;
	  	std::vector<Function *> allFunctions;
	  	  	
		for (auto &F : M) {
			if (F.size() > 0 && !F.isDeclaration()) {
				allFunctions.push_back(&F);
			}	
		}
		
	  	for (auto funcName : functionsToProtect) {
	  		Function *F = M.getFunction(funcName);
	  		if (F == nullptr || F->size() <= 0 || F->isDeclaration()) {
	  			errs() << WARNING << "Function " << funcName << " not found and will be skipped!\n";
	  			continue;
	  		}
	  		
	  		pureFunctions.push_back(F);
	  	}
	  	
	  	Graph g = createCheckerNetwork(allFunctions, pureFunctions);
	  	
	  	std::pair<vertex_iter, vertex_iter> vp;
    
		// For every basic block from the graph
		for (vp = vertices(g); vp.first != vp.second; ++vp.first) {
			VertexDescriptor v = *vp.first;
			Function *checkerFunction = g[v].function;
			errs() << "Function " << checkerFunction->getName() << " checks \n"; 
			std::pair<adj_vertex_iter, adj_vertex_iter> adj_vp;
	  		
	  		for (adj_vp = adjacent_vertices(v, g); adj_vp.first != adj_vp.second; ++adj_vp.first) {
				v = *adj_vp.first;
				Function *checkeeFunction = g[v].function;
				errs() << "\t" << checkeeFunction->getName() << "\n"; 
				// Insert checker
				insertProtect(M, checkerFunction, checkeeFunction);
			}
		}
	  	return true;		
	}
}

char StateProtectorPass::ID = 0;

RegisterPass<StateProtectorPass> X("stateProtect", "State Protector Pass", false, false);

