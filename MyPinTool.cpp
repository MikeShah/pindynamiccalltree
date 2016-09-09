#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include "pin.H"
#include <cstdlib>
#include <sstream>
#include <stack>
#include <map>

#include "output.h"

outputFile o;
PIN_LOCK lock;

std::vector<std::string>* filesToInstrument;
// Maps an instruction address to the filename that the function call belongs to.
std::map<ADDRINT, std::string> addressToFileNameMap;

// Should we log which thread a call may have been made from? (We do not know precisely which one though!)
bool enableThreading = false;
// Should we only analyze our .exe or our .dll?
bool ignoreSharedLibraries = true;
// Analyze all files
// For all .exe or .dll's loaded, do we analyze all files or just specific ones?
bool analyzeAllFiles = true;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "malloc_mt.out", "specify output file name");

VOID calledMe(ADDRINT instruction_address, ADDRINT target_address, ADDRINT fileName)
{

	PIN_LockClient();

		std:stringstream s;
		RTN ins_rtn = RTN_FindByAddress(instruction_address);
		RTN tar_rtn = RTN_FindByAddress(target_address);
		short state = 0;

		o.indentation[0]++;
		for (int i = 0; i < o.indentation[0]; ++i) {
			s << ".";
		}

			if (RTN_Valid(tar_rtn) && RTN_Valid(ins_rtn)) {
				std::string ins_symbolName = RTN_Name(ins_rtn);
				std::string tar_symbolName = RTN_Name(tar_rtn);
				ins_symbolName = PIN_UndecorateSymbolName(ins_symbolName, UNDECORATION_COMPLETE);
				tar_symbolName = PIN_UndecorateSymbolName(tar_symbolName, UNDECORATION_COMPLETE);
				s << std::hex << ins_symbolName << " => " << tar_symbolName << " in " << addressToFileNameMap[fileName] << std::endl;
				state = 0;
			}
			else if (RTN_Valid(tar_rtn) && !RTN_Valid(ins_rtn)) {
				std::string tar_symbolName = RTN_Name(tar_rtn);
				tar_symbolName = PIN_UndecorateSymbolName(tar_symbolName, UNDECORATION_COMPLETE);
				s << std::hex << instruction_address << " => " << tar_symbolName << std::endl;
				state = 1;
			}
			else if (!RTN_Valid(tar_rtn) && RTN_Valid(ins_rtn)) {
				std::string ins_symbolName = RTN_Name(ins_rtn);
				ins_symbolName = PIN_UndecorateSymbolName(ins_symbolName, UNDECORATION_COMPLETE);
				s << std::hex << ins_symbolName << " => " << target_address << " in " << addressToFileNameMap[fileName] << std::endl;
				state = 2;
			}
			else if (!RTN_Valid(tar_rtn) && !RTN_Valid(ins_rtn)) {
				s << std::hex << instruction_address << " => " << target_address << std::endl;
				state = 3;
			}

			o.quickWrite(s.str());

	PIN_UnlockClient();
}

VOID RemoveIndent() {
	o.indentation[0]--;
}

// Begin instrumenting routine with additional code.
VOID modifyRoutine(RTN rtn) {
	std::stringstream s;
	std::string filename;
	INT32 line;

	// Find the current image we are using
	IMG img = SEC_Img(RTN_Sec(rtn));

	// Begin modifying routine
	RTN_Open(rtn);
	INS insHead = RTN_InsHead(rtn);		// Insert at the head of the instruction
	INS insTail = RTN_InsTail(rtn);	// Insert at the tail of the instruction

	PIN_GetSourceLocation(INS_Address(insHead), NULL, &line, &filename);
	s << "---" << filename << "\t" << line << "\t" << RTN_Name(rtn) << "\tIMG " << IMG_Name(img) << "\n";
	// Store in a map which address maps to which filename
	std::pair<ADDRINT, std::string> item;
	item.first = INS_Address(insHead);
	item.second = filename;
	addressToFileNameMap.insert(item);

	if (analyzeAllFiles) {
		INS_InsertCall(insHead, IPOINT_BEFORE, (AFUNPTR)calledMe, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_INST_PTR, IARG_END);
		INS_InsertCall(insTail, IPOINT_BEFORE, (AFUNPTR)RemoveIndent, IARG_END);
	}
	else {
		for (std::vector<std::string>::iterator it = filesToInstrument->begin(); it != filesToInstrument->end(); ++it) {
			if (filename.find(*it) != std::string::npos) {
				// Output which routines/funcitons/procedures are getting instrumented
				if (insHead.is_valid() && insTail.is_valid()) {
					//if (INS_HasFallThrough(ins)) {
					INS_InsertCall(insHead, IPOINT_BEFORE, (AFUNPTR)calledMe, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_INST_PTR, IARG_END);
					INS_InsertCall(insTail, IPOINT_BEFORE, (AFUNPTR)RemoveIndent, IARG_END);
					//}
				}
			}
		}
	}



	// Finish instrumenting routine
	RTN_Close(rtn);
	o.quickWrite(s.str());
}

// Insert calll instructions
VOID findCalls(RTN rtn, VOID *v) {

	THREADID m_threadID = PIN_ThreadId() + 1;
	if (RTN_Valid(rtn)) {

		// Filter out to only include our .exe
		// This means we are ignoring shared libraries.
		if (ignoreSharedLibraries){
			if (IMG_Name(SEC_Img(RTN_Sec(rtn))).find(".exe") != std::string::npos) {
				modifyRoutine(rtn);
			}
		}else {
			modifyRoutine(rtn);
		}
	}
}

// Note that opening a file in a callback is only supported on Linux systems.
// See buffer-win.cpp for how to work around this issue on Windows.
//
// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	std::stringstream s;
	s << "============== Thread Begin ============== " << PIN_ThreadId() << std::endl;
	o.quickWrite(s.str());
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	std::stringstream s;
	s << "============== Thread End ============== " << PIN_ThreadId() << std::endl;
	o.quickWrite(s.str());
}

// This routine is executed each time malloc is called.
VOID BeforeMalloc(int size, THREADID threadid)
{
	//PIN_GetLock(&lock, threadid + 1);
	//outFile << "thread " << threadid << " entered malloc(" << size << ")" << std::endl;
	//quickWrite("thread entered malloc\n");
	//PIN_ReleaseLock(&lock);
}

//====================================================================
// Instrumentation Routines
//====================================================================

// This routine is executed once at the end.
VOID Fini(INT32 code, VOID *v)
{
	std::stringstream ss;

	ss  << setw(23) << "Procedure" << " "
		<< setw(15) << "Image" << " "
		<< setw(18) << "Address" << " "
		<< setw(12) << "Calls" << " "
		<< setw(12) << "Instructions" << endl;

	o.quickWrite(ss.str());

	o.quickWrite("\n===============^ Ending Pin Instrumented Program ^==============="); 
	o.flush();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
	PIN_ERROR("This is a custom Pin Tool by Mike\n" + KNOB_BASE::StringKnobSummary() + "\n");
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
int main(INT32 argc, CHAR **argv)
{
	// Setup our datastructure
	filesToInstrument = new std::vector<std::string>();
	filesToInstrument->push_back("clear.cpp");

	// Must be called so that symbol table information is available.
	// According to the IN Guide, this needs to be called before
	// PIN_Init
	PIN_InitSymbols();
	
	// Initialize pin
	if (PIN_Init(argc, argv)) {
		return Usage();
	}

  	string fileName = KnobOutputFile.Value();

	o.quickWrite("=============== v Starting Pin Instrumented Program v ===============\n");

    // Register Routine to be called to instrument rtn
	RTN_AddInstrumentFunction(findCalls, 0);

	

	if (enableThreading) {
		// Register Analysis routines to be called when a thread begins/ends
		PIN_AddThreadStartFunction(ThreadStart, 0);
		PIN_AddThreadFiniFunction(ThreadFini, 0);
	}

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
	
    // Never returns
    PIN_StartProgram();
    
    return 0;
}