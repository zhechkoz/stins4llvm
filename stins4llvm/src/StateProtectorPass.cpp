#include "llvm/Pass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Support/raw_ostream.h"

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
		std::vector<std::string> functionsToProtect = {"max"};
		const std::string program = "/home/zhechev/Developer/SIP/phase3/stins4llvm/src/InterestingProgram.c";
		const std::string syminput = "../Docker/klee/syminputC.py";
		
		Json::Value pureFunctionsTestCases;
		 
		StateProtectorPass() : ModulePass(ID) {}
		
		bool doInitialization(Module &M) override;
		bool runOnModule(Module &M) override;	
		
		Json::Value parse(char * input);
    };
    
	bool StateProtectorPass::doInitialization(Module &M) {
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
	
	void insertProtect(Module &M, Function *checker, Function *checkee) {
		BasicBlock *firstBasicBlock = &checker->getEntryBlock();
		BasicBlock *reportBlock = BasicBlock::Create(M.getContext(), "reportBlock", checker, firstBasicBlock);
		BasicBlock *funcNotOnStack = BasicBlock::Create(M.getContext(), "funcNotOnStack", checker, reportBlock);
		
		
		IRBuilder<> builder(funcNotOnStack);
		
		
		std::vector<Value *> args;
		args.push_back(builder.getInt32(42));
		args.push_back(builder.getInt32(42));
		args.push_back(builder.getInt32(42));
		
		
		Value *checkeeCall = builder.CreateCall(checkee, args);
		Value *checkerResult = builder.CreateICmpNE(builder.getInt32(42), checkeeCall);
		builder.CreateCondBr(checkerResult, firstBasicBlock, reportBlock);
		
		builder.SetInsertPoint(reportBlock);
		Constant *reportFunction = M.getOrInsertFunction("report", 
					FunctionType::get(Type::getVoidTy(M.getContext()), false));
		builder.CreateCall(reportFunction);
		builder.CreateUnreachable();
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
				//TODO: Insert checker
				insertProtect(M, checkerFunction, checkeeFunction);
			}
		}
	  	return true;		
	}
}

char StateProtectorPass::ID = 0;

RegisterPass<StateProtectorPass> X("stateProtect", "State Protector Pass", false, false);

