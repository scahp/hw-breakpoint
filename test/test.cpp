#include <Windows.h>
#include <functional>
#include <iostream>
#include "..\breakpoint.h"

// extending console output with colors
enum ConsoleColor
{
	White = 7,
	Green = 10,
	Red = 12
};
std::ostream& operator<<(std::ostream& os, ConsoleColor color)
{
	static HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
	::SetConsoleTextAttribute(hConsole, color);
	return os;
};

// globals
int g_val = 0;
HANDLE g_hEvent1, g_hEvent2;

enum TestType
{
	Write,
	Read
};

auto defTryWriteFunc = []() { std::cout << "\tmissed writes " << Red << "[failed]" << White << std::endl; };
auto defExceptWriteFunc = []() { std::cout << "\tcatch write attempt " << Green << "[ok]" << White << std::endl; };
auto defTryReadFunc = []() { std::cout << "\tmissed read " << Red << "[failed]" << White << std::endl; };
auto defExceptReadFunc = []() { std::cout << "\tcatch read attempt " << Green << "[ok]" << White << std::endl; };

void tryTest(TestType testType, std::function<void()>& tryFunc, std::function<void()>& exceptFunc)
{
	__try
	{
		switch (testType)
		{
		case Write:
			std::cout << White << "thread " << std::hex << ::GetCurrentThreadId() << " trying to write...";
			g_val = 1;
			break;
		case Read:
			std::cout << White << "thread " << std::hex << ::GetCurrentThreadId() << " trying to read...";
			volatile int read = g_val;
			break;
		}
		tryFunc();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		exceptFunc();
	}
}

void test(TestType testType, std::function<void()> tryFunc = std::function<void()>(), std::function<void()> exceptFunc = std::function<void()>())
{
	if (!tryFunc)
	{
		switch (testType)
		{
		case Write: tryFunc = defTryWriteFunc; break;
		case Read: tryFunc = defTryReadFunc; break;
		default: return;
		}
	}

	if (!exceptFunc)
	{
		switch (testType)
		{
		case Write: exceptFunc = defExceptWriteFunc; break;
		case Read: exceptFunc = defExceptReadFunc; break;
		default: return;
		}
	}

	tryTest(testType, tryFunc, exceptFunc);
};

DWORD WINAPI ThreadWriteFunc(TestType param)
{
    	// inform main thread to set breakpoint
	::SetEvent(g_hEvent2);

    	// wait for main thread to finish setting the BP
	::WaitForSingleObject(g_hEvent1, INFINITE);

	test(param);

	return 0;
}

int main()
{
	// test write
	std::cout << "\n\ntest 1: testing write only BP";
	std::cout << "\n=============================\n";
	HWBreakpoint::Set(&g_val, sizeof(int), HWBreakpoint::Write);
	test(TestType::Write);
	test(TestType::Read, 
		[]() { std::cout << "\tmissed read " << Green << "[ok]" << White << std::endl; },
		[]() { std::cout << "\tcatch read attempt " << Red << "[failed]" << White << std::endl; 
	});

	// test read & write
	std::cout << "\n\ntest 2: testing read & write BP";
	std::cout << "\n===============================\n";
	HWBreakpoint::Set(&g_val, sizeof(int), HWBreakpoint::ReadWrite);
	test(TestType::Write);
	test(TestType::Read);

	// test clear
	std::cout << "\n\ntest 3: clearing BP";
	std::cout << "\n===================\n";
	HWBreakpoint::Clear(&g_val);
	test(TestType::Write, 
		[]() { std::cout << "\tmissed write " << Green << "[ok]" << White << std::endl;}, 
		[]() { std::cout << "\tcatch write attempt " << Red << "[failed]" << White << std::endl; 
	});

	HANDLE hTrd;
	DWORD threadId;

	g_hEvent1 = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hEvent2 = ::CreateEvent(NULL, TRUE, FALSE, NULL);

    	// multi-thread testing:
	std::cout << "\n\ntest 4: existing thread before the BP has setting";
	std::cout <<   "\n=================================================" << std::endl;
	hTrd = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadWriteFunc, (LPVOID)TestType::Write, 0, &threadId);
	
    	// wait for new thread creation
    	::WaitForSingleObject(g_hEvent2, INFINITE);

    	// print out the new thread id
	std::cout << "thread " << std::hex << threadId << " has created" << std::endl;

    	// set the BP
	HWBreakpoint::Set(&g_val, sizeof(int), HWBreakpoint::Write);

    	// signal the thread to continue exection (try to write)
	::SetEvent(g_hEvent1);

    	// wait for thread completion
	::WaitForSingleObject(hTrd, INFINITE);

        // cleanup and reset events
	::CloseHandle(hTrd);
	::ResetEvent(g_hEvent1);
	::ResetEvent(g_hEvent2);

	std::cout << "\n\ntest 5: new thread after setting the BP";
	std::cout <<   "\n=======================================" << std::endl;
	hTrd = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadWriteFunc, (LPVOID)TestType::Write, 0, &threadId);

    	// wait for new thread creation
	::WaitForSingleObject(g_hEvent2, INFINITE);

    	// print out the new thread id
	std::cout << "thread " << std::hex << threadId << " has created" << std::endl;

    	// signal the thread to continue execution
	::SetEvent(g_hEvent1);

    	// wait for thread completion
	::WaitForSingleObject(hTrd, INFINITE);
	::CloseHandle(hTrd);

    	// reset the BP
	HWBreakpoint::Clear(&g_val);

    	// wait for user input
	std::cin.ignore();
}
