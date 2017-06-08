#include "llvm/Pass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APFloat.h"

#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <iostream>
#include <algorithm>

#include <Python.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>

#define WARNING "\033[43m\033[1mWARNING:\033[0m "
#define ERROR "\033[101m\033[1mERROR:\033[0m "

#define PYTHON_OUTPUT_PATH "/tmp/klee.json"

using namespace llvm;

// Network of checkers using boost
struct Vertex { Function *function; };
struct Edge { std::string blah; };

typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, Vertex, Edge> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<Graph>::edge_descriptor edge_t;
typedef Graph::vertex_descriptor VertexDescriptor;
typedef Graph::vertex_iterator vertex_iter;
typedef Graph::adjacency_iterator adj_vertex_iter;

namespace {
	enum FunctionsToInsertNames { checkTrace = 0, report, cmpstr, initRandom };
	const std::vector<std::string> FunctionsToInsert = { "checkTrace" , "report", "cmpstr", "initRandom" };

	const std::string RESULT = "result", PARAMETER = "parameter";
	const std::vector<std::string> ENTRYPOINTS = {"main"};

	const std::string USAGE = "Specify file containing configuration file!";
	const cl::opt<std::string> FILENAME("ff", cl::desc(USAGE.c_str()));

	struct StateProtectorPass : public ModulePass {
		// Variables
		static char ID;
		Constant *checkFunction, *reportFunction;

		std::vector<std::string> functionsToProtect;
		std::string inputProgram, syminput;
		int connectivity = 0;
		bool verbose = false;

		Type *x86_FP80Ty, *FP128Ty, *boolTy, *strPtrTy, *voidTy;

		Json::Value pureFunctionsTestCases;

		StateProtectorPass() : ModulePass(ID) {}

		// Functions
		bool doInitialization(Module &M) override;
		bool runOnModule(Module &M) override;

		Json::Value generateTestCasesForFunctions(std::vector<Function *> functions,
											size_t maxFunctionNameLength);

		Graph createCheckerNetwork(std::vector<Function *> allFunctions,
							   std::vector<Function *> pureFunctions, unsigned int connectivity);

		Value* createLLVMValue(IRBuilder<> *builder, std::string value, Type *type);
		void insertProtect(Module &M, Function *checker, Function *checkee);
		void insertRandSeed(Module &M);
		Json::Value parseFromFile(std::string fileName);
		void parseConfig(std::string fileName);
	};

	void StateProtectorPass::parseConfig(std::string fileName) {
		Json::Value config = parseFromFile(fileName); // Parse config file

		inputProgram = config["program"].asString();
		syminput = config["syminput"].asString();
		connectivity = config["connectivity"].asInt();
		verbose = config["verbose"].asBool();

		for (auto function : config["functions"]) {
			functionsToProtect.push_back(function.asString());
		}

		if (functionsToProtect.empty() || inputProgram.empty() || syminput.empty() || connectivity <= 0) {
			errs() << ERROR << "Not initialised correctly! Make sure "
							<< "you provide at least one pure function to protect!\n";
			exit(1);
		}
	}

	bool StateProtectorPass::doInitialization(Module &M) {
		// Check if there is a function in the target program that conflicts
		// with the current set of functions
		for (auto functionName : FunctionsToInsert) {
			if (M.getFunction(StringRef(functionName)) != nullptr) {
				errs() << ERROR << " The target program should not contain function called "
					   << functionName << "\n";
				exit(1);
			}
		}

		std::srand(std::time(0));

		LLVMContext &ctx = M.getContext();

		x86_FP80Ty = Type::getX86_FP80Ty(ctx);
		FP128Ty = Type::getFP128Ty(ctx);
		boolTy = Type::getInt1Ty(ctx);
		strPtrTy = Type::getInt8PtrTy(ctx);
		voidTy = Type::getVoidTy(ctx);

		parseConfig(FILENAME);

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

			errs() << "==== TEST CASES ====\n\n" << output << "\n\n";
		}

		return false;
	}

	bool StateProtectorPass::runOnModule(Module &M) {
		std::vector<Function *> pureFunctions,  allFunctions;

		for (auto &F : M) {
			if (F.size() > 0 && !F.isDeclaration()) {
				allFunctions.push_back(&F);

				if (std::find(functionsToProtect.begin(), functionsToProtect.end(), F.getName())
						!= functionsToProtect.end() && pureFunctionsTestCases.isMember(F.getName())) {
					pureFunctions.push_back(&F);
				}
			}
		}

	  	if (!pureFunctions.empty()) {
	  		errs() << "Pure functions which will be protected: \n";

	  		for (auto function : pureFunctions) {
	  			errs() << "\t" << function->getName() << "\n";
	  		}
	  		errs() << "\n";
	  	}

	  	Graph g = createCheckerNetwork(allFunctions, pureFunctions, connectivity);

	  	std::pair<vertex_iter, vertex_iter> vp;

		// For every function from the graph
		for (vp = vertices(g); vp.first != vp.second; ++vp.first) {
			VertexDescriptor v = *vp.first;
			Function *checkerFunction = g[v].function;

			std::pair<adj_vertex_iter, adj_vertex_iter> adj_vp = adjacent_vertices(v, g);

			// Function checks other functions
	  		if (adj_vp.first != adj_vp.second) {
	  			errs() << "Function " << checkerFunction->getName() << " checks: \n";
	  		}

	  		for ( ;adj_vp.first != adj_vp.second; ++adj_vp.first) {
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

	Json::Value StateProtectorPass::generateTestCasesForFunctions(std::vector<Function *> functions,
																				   size_t maxFunctionNameLength) {
		Json::Value outputTestCases;
		FILE *file;
		int argc = 3, returnValue;
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

			if (file == NULL) {
				errs() << ERROR << "Test generation script not found!\n";
				exit(1);
			}

			returnValue = PyRun_SimpleFile(file, syminput.c_str());
			Py_Finalize();

			fclose(file);

			if (returnValue == 0) {
				// Save intermediate results from parsing
				Json::Value currentFunctionTestCases = parseFromFile(PYTHON_OUTPUT_PATH);
				outputTestCases[functionName] = currentFunctionTestCases;
			} else {
				errs() << WARNING << "Failed to generate test cases for function " << functionName << "\n";
			}
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

		if (!file.good()) {
			errs() << ERROR << "File " << fileName << " could not be found!\n";
			exit(1);
		}

		bool parsingSuccessful = reader.parse(file, root, false);

		if (!parsingSuccessful) {
			errs()  << WARNING << "Failed to parse file " << fileName << " correctly!\n"
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
			while (out < connectivity) {
				vertex_t checker;

				// Checkers are chosen randomly
				int rand_pos;
				while (true) {
					rand_pos = rand() % checkerFunctions.size();
					checker = checkerFunctions[rand_pos];
					if (checkee != checker) {
						break;
					}
				}

				// If the edge was added increase the counter and repeat until connectivity is reached
				if (boost::add_edge(checker, checkee, g).second) {
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
		// Cast a value (argument, return type) into an LLVM type that can be used with IRBuilder
		Value *output = nullptr;

		if (type->isPointerTy()) {
			Type *content = type->getContainedType(0);
			// A Pointer type pointing to 8 bit values is a pointer of chars
			if (content->isIntegerTy(8)) {	 // char*
				int length = value.length();
				if ((length == 4 || length == 8 || length == 16) && value.substr(0, 2) == "0x") {
					std::string result;
					for (int i = length - 2; i > 1; i-=2) {
						std::string byte = value.substr(i,2);
						char chr = (char) (int) std::strtol(byte.c_str(), nullptr, 16);
						result.push_back(chr);
					}
					output = builder->CreateGlobalStringPtr(result);
 				} else {
 					output = builder->CreateGlobalStringPtr(value);
 				}
			} else {
				errs() << ERROR << "Type ";
				type->print(errs());
				errs() << " cannot be resolved with the current implementation!\n";
			}
		} else if (type->isIntegerTy()) {
			// Integer types can be of length 8 bit (char), 16 bit (short), 32 bit (int), 64 bit (long) and 128 bit (long long)
			if (type->isIntegerTy(8)) {
				output = builder->getInt8(value[0]);
			} else if (type->isIntegerTy(16)) {
				short castedShort = (short) std::stoi(value, nullptr, 16);
				output = builder->getInt16(castedShort);
			} else if (type->isIntegerTy(32)) {
				int castedInt = std::stoi(value, nullptr, 16);
				output = builder->getInt32(castedInt);
			} else if (type->isIntegerTy(64)) {
				long castedLong = std::stol(value, nullptr, 16);
				output = builder->getInt64(castedLong);
			} else if (type->isIntegerTy(128)) {
				long long castedLongLong = std::stoll(value, nullptr, 16);
				output = builder->getIntN(128, castedLongLong);
			} else {
				errs() << ERROR << "Type ";
				type->print(errs());
				errs() << " cannot be resolved with the current implementation!\n";
			}
		} else if (type->isFloatingPointTy()) {
			// Floating point types can be of length 32 bit (float), 64 bit (double) and 80/128 bit (long double)
			if (type->isFloatTy()) {
				float castedFloat = std::stoi(value, nullptr, 16);
				output = ConstantFP::get(builder->getFloatTy(), castedFloat);
			} else if (type->isDoubleTy()) {
				double castedDouble = std::stol(value, nullptr, 16);
				output = ConstantFP::get(builder->getDoubleTy(), castedDouble);
			} else if (type->isX86_FP80Ty()) {
				long double castedDouble = std::stoll(value, nullptr, 16);
				output = ConstantFP::get(x86_FP80Ty, castedDouble);
			} else if (type->isFP128Ty()) {
				long double castedDouble = std::stoll(value, nullptr, 16);
				output = ConstantFP::get(FP128Ty, castedDouble);
			} else {
				// Print an error if a type was found that is not supported
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
		std::vector<Value *> args;
		BasicBlock *firstBasicBlock = &checker->getEntryBlock();

		// Create block that calls the report function
		BasicBlock *reportBlock = BasicBlock::Create(M.getContext(), "reportBlock", checker, firstBasicBlock);

		// Create basic block for each test case of a checkee
		std::vector<BasicBlock *> funcNotOnStack;
		for (unsigned int j = 0; j < pureFunctionsTestCases[checkee->getName()].size(); j++) {
			BasicBlock *checkBlock = BasicBlock::Create(M.getContext(), "funcNotOnStack", checker, reportBlock);
			funcNotOnStack.push_back(checkBlock);
		}

		// Create basic block to check if the checkee is on the call stack -> prevent circles
		BasicBlock *funcOnStackTest = BasicBlock::Create(M.getContext(), "funcOnStack", checker, funcNotOnStack[0]);

		IRBuilder<> builder(funcOnStackTest);
		Constant *stackFunction = M.getOrInsertFunction(FunctionsToInsert[checkTrace],
											FunctionType::get(boolTy, strPtrTy, false));

		std::string functionNameString = checkee->getName();
		Value *functionName = builder.CreateGlobalStringPtr(functionNameString);

		// Call a check function from a library if the checkee is on the stack
		args.push_back(functionName);
		Value *stackCall = builder.CreateCall(stackFunction, args);

		Value *onStack = builder.getInt1(true);
		Value *stackResult = builder.CreateICmpEQ(onStack, stackCall);
		builder.CreateCondBr(stackResult, firstBasicBlock, funcNotOnStack[0]);


		Value *checkeeCall;
		Value *result;
		Value *checkerResult;

		// Call the checkee once for each test case, compare the results and jump to the report function if necessar<
		for (unsigned int j = 0; j < pureFunctionsTestCases[checkee->getName()].size(); j++) {
			// Use an own basic block for each test case
			BasicBlock *checkBlock = funcNotOnStack[j];
			builder.SetInsertPoint(checkBlock);
			args.clear();

			// Cast all arguments to LLVM values using the checkee signature
			int i = 0;
			for (auto &argument : checkee->getArgumentList()) {
				std::string testParameter = pureFunctionsTestCases[checkee->getName()][std::to_string(j)][PARAMETER][i].asString();
				Value *arg = createLLVMValue(&builder, testParameter, argument.getType());
				args.push_back(arg);
				i++;
			}

			// Create the checkee call with the arguments
			checkeeCall = builder.CreateCall(checkee, args);
			std::string testResult = pureFunctionsTestCases[checkee->getName()][std::to_string(j)][RESULT].asString();
			result = createLLVMValue(&builder, testResult, checkee->getReturnType());

			// Compare the result to the expected value
			if (checkee->getReturnType()->isIntegerTy()) {
				//Insert a comparison for Integer type results
				checkerResult = builder.CreateICmpNE(result, checkeeCall);
			} else if (checkee->getReturnType()->isFloatingPointTy()) {
				// Insert a comparision for Floatingpoint type results
				checkerResult = builder.CreateFCmpONE(result, checkeeCall);
			} else if (checkee->getReturnType()->isPointerTy() &&
						checkee->getReturnType()->getContainedType(0)->isIntegerTy(8)) { // char *
				// Insert a checker that compares the values the pointers reference to
				// The comparison is done by externel functions included form libcheck.cpp
				// so far only a function to compare the values of char pointer is available
				Type *argsTypes[2] = {strPtrTy, strPtrTy};

				Constant *strcmpFunction = M.getOrInsertFunction(FunctionsToInsert[cmpstr],
						FunctionType::get(boolTy, ArrayRef<Type *>(argsTypes), false));

				args.clear();
				args.push_back(result);
				args.push_back(checkeeCall);
				result = builder.CreateCall(strcmpFunction, args);
				checkerResult = builder.CreateICmpNE(result, builder.getInt1(false));
			}

			// Call the report function if a check was negative or jump to the next basic block (checker/ original function) if the test was positive
			if (j == pureFunctionsTestCases[checkee->getName()].size() - 1) {
				builder.CreateCondBr(checkerResult, reportBlock, firstBasicBlock);
			} else {
				builder.CreateCondBr(checkerResult, reportBlock, funcNotOnStack[j + 1]);
			}
		}

		// Insert a call to the report function into the creared basic block that can be called from checker functions
		builder.SetInsertPoint(reportBlock);
		Constant *reportFunction = M.getOrInsertFunction(FunctionsToInsert[report],
											FunctionType::get(voidTy, false));
		builder.CreateCall(reportFunction);
		builder.CreateBr(firstBasicBlock);
	}

	void StateProtectorPass::insertRandSeed(Module &M) {
		Function *F = nullptr;

		// Find a valid function to insert the seedRandom
		for (auto function : ENTRYPOINTS) {
			F = M.getFunction(function);

			// Found a function to insert
			if (F != nullptr && F->size() > 0 && !F->isDeclaration()) { break; }
		}

		if (F != nullptr && F->size() > 0 && !F->isDeclaration()) {
			Instruction *firstInst = &*(F->getEntryBlock().getFirstInsertionPt());

			IRBuilder<> builder(firstInst);

			Constant *initRandomFunction = M.getOrInsertFunction(FunctionsToInsert[initRandom],
													FunctionType::get(voidTy, false));
			builder.CreateCall(initRandomFunction);
		} else {
			errs() << WARNING << "Random function cannot be seeded!\n";
		}
	}
}

char StateProtectorPass::ID = 0;

RegisterPass<StateProtectorPass> X("stateProtect", "State Protector Pass", false, false);
