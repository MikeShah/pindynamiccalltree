#pragma once

#include "pin.H"


class outputFile {
public:

	static const long long OUT_BUFFER_SIZE = 50;
	long long itemsInOutBuffer ;
	const char* outBuffer[OUT_BUFFER_SIZE];

	PIN_LOCK lock;
	static const int numThreads = 500;
	int* indentation;

	// lock serializes access to the output file.
	ofstream o;

	// Constructor
	outputFile() {

		// Initialize the pin lock
		PIN_InitLock(&lock);

		// Setup buffer
		itemsInOutBuffer = 0;
		indentation = new int[numThreads];

		o.open("proccount.out");

		// Zero out our values
		for (int i = 0; i < numThreads; ++i) {
			indentation[i] = 0;
		}

		// Zero out values
		for (int i = 0; i < OUT_BUFFER_SIZE; ++i) {
			outBuffer[i] = NULL;
		}
	}

	// Destructor
	~outputFile() {
		o.close();
		delete[] indentation;
	}

	// Flush and finish
	void flush() {
		o.flush();
	}

	VOID indent(THREADID m_threadID, std::string* funcName) {

		PIN_GetLock(&lock, m_threadID + 1);
		indentation[m_threadID]++;
		//std::string s = "[thread ] |" + spaceString(m_threadID) + *funcName + "( )";
		writeOutBuffer();
		PIN_ReleaseLock(&lock);
	}

	// This function is called before every instruction is executed
	VOID unindent(THREADID m_threadID, std::string* funcName)
	{
		PIN_GetLock(&lock, m_threadID + 1);
		//o << *funcName << "(" << ") from thread " << indentation[m_threadID] << " exited" << std::endl;
		indentation[m_threadID]--;
		PIN_ReleaseLock(&lock);
	}

	void writeOutBuffer() {
		if (itemsInOutBuffer > OUT_BUFFER_SIZE - 1) {
			for (int i = 0; i < OUT_BUFFER_SIZE; ++i) {
				o.write(outBuffer[i], itemsInOutBuffer);
			}
			itemsInOutBuffer = 0;
		}
	}

	// Called at the end of the program, such that
	// all contents of the buffer get emitted.
	void writeRemainingBuffer() {
		for (int i = 0; i < itemsInOutBuffer; ++i) {
			o.write(outBuffer[i], itemsInOutBuffer);
		}
	}

	// Write a string to a buffer
	void writeToBuffer(std::string s) {
		outBuffer[itemsInOutBuffer] = "s";//s.c_str();
		itemsInOutBuffer++;
		writeOutBuffer();
	}

	// Writes directly to our outfile without buffering
	void quickWrite(std::string s) {
		THREADID m_threadID = PIN_ThreadId() + 1;

		PIN_GetLock(&lock, m_threadID + 1);
			o.flush();
			o.write(s.c_str(), (strlen(s.c_str())) * sizeof(char));
			o.flush();
		PIN_ReleaseLock(&lock);
	}

	// Writes directly to our outfile without buffering
	void quickWrite(const char* s) {
		THREADID m_threadID = PIN_ThreadId() + 1;

		PIN_GetLock(&lock, m_threadID + 1);
		o.flush();
		o.write(s, (strlen(s)) * sizeof(char));
		o.flush();
		PIN_ReleaseLock(&lock);
	}

	std::string spaceString(THREADID m_threadID) {
		//for (int i = 0; i < indentation[m_threadID]; ++i) {
		//	outFile << ".";
		//}
		return ".";
	}
};


