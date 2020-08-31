/* guru.h -- Guru error-handling and reporting system.
   RELEASE VERSION 1.3 -- 31st August 2020

MIT License

Copyright (c) 2019-2020 Raine "Gravecat" Simmons.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

// Uncomment either ONE or NONE of these lines, depending on if you are using PDCurses/NCurses, or if your application is a non-Curses console app, or if you are using libtcod.
//#define GURU_USING_CURSES		// Uncomment this only if you are using PDCurses or NCurses.
//#define GURU_USING_CONSOLE	// Uncomment this only if you are compiling a console-based application which is NOT using PDCurses or NCurses.
//#define GURU_USING_LIBTCOD		// Uncomment this onyl if you are using libtcod.

// Comment out this line if you DO NOT want to use Guru's stack-trace system.
//#define GURU_USING_STACK_TRACE

#include <exception>
#ifdef GURU_USING_STACK_TRACE
#include <stack>
#endif
#include <string>


namespace guru
{

#ifdef GURU_USING_STACK_TRACE
// The stack-trace system. The advantage of this over traditional debug methods is that we can still strip symbol information (to keep the binary size down),
// and it'll generate useful information in the log file even for regular players, rather than only when compiled/running in 'debug mode'.
struct StackTrace
{
	StackTrace(const char *func);
	~StackTrace();
	static std::stack<const char*>	funcs;
};
#define stack_trace()	guru::StackTrace local_stack(__PRETTY_FUNCTION__)
#endif


#define GURU_INFO		0	// General logging information.
#define GURU_WARN		1	// Warnings, non-fatal stuff.
#define GURU_ERROR		2	// Serious errors. Shit is going down.
#define GURU_CRITICAL	3	// Critical system failure.
#ifdef GURU_USING_STACK_TRACE
#define GURU_STACK		4	// Stack traces.
#endif

void	affirm(int condition, std::string error);	// Like assert(), but calls a Guru halt() if the condition is false.
void	close_syslog();				// Closes the Guru log file.
void	console_ready(bool ready);	// Tells Guru whether or not the console is initialized and can handle rendering error messages.
void	halt(std::string error, ...);	// Stops the game and displays an error messge.
void	halt(std::exception &e);	// As above, but with an exception instead of a string.
void	intercept_signal(int sig);	// Catches a segfault or other fatal signal.
void	log(std::string msg, int type = GURU_INFO, ...);	// Logs a message in the system log file.
void	nonfatal(std::string error, int type, ...);	// Reports a non-fatal error, which will be logged but will not halt execution unless it cascades.
void	open_syslog(std::string filename = "");	// Opens the output log for messages.

}	// namespace guru
