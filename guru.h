/* guru.h -- Guru error-handling and reporting system.
 * RELEASE VERSION 1.0 -- 12th December 2019

MIT License

Copyright (c) 2019 Raine Simmons.

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

// Uncomment either ONE or NEITHER of these lines, depending on if you are using PDCurses/NCurses, or if your application is a non-Curses console app.
//#define GURU_USING_CURSES		// Uncomment this only if you are using PDCurses or NCurses.
//#define GURU_USING_CONSOLE	// Uncomment this only if you are compiling a console-based application which is NOT using PDCurses or NCurses.

#include <stack>
#include <string>

// The stack-trace system. The advantage of this over traditional debug methods is that we can still strip symbol information (to keep the binary size down),
// and it'll generate useful information in the log file even for regular players, rather than only when compiled/running in 'debug mode'.
struct StackTrace
{
	StackTrace(const char *func);
	~StackTrace();
	static std::stack<const char*> funcs;
};
#define stack_trace()	StackTrace local_stack(__PRETTY_FUNCTION__)


#define GURU_INFO		0	// General logging information.
#define GURU_WARN		1	// Warnings, non-fatal stuff.
#define GURU_ERROR		2	// Serious errors. Shit is going down.
#define GURU_CRITICAL	3	// Critical system failure.
#define GURU_STACK		4	// Stack traces.


namespace guru
{

void	close_syslog();				// Closes the Guru log file.
void	console_ready(bool ready);	// Tells Guru whether or not the console is initialized and can handle rendering error messages.
void	halt(std::string error);	// Stops the game and displays an error messge.
void	intercept_signal(int sig);	// Catches a segfault or other fatal signal.
void	log(std::string msg, int type = GURU_INFO);	// Logs a message in the system log file.
void	nonfatal(std::string error, int type);	// Reports a non-fatal error, which will be logged and displayed in-game but will not halt execution unless it cascades.
void	open_syslog();				// Opens the output log for messages.

}	// namespace guru
