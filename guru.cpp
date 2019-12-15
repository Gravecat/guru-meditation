/* guru.h -- Guru error-handling and reporting system.
   RELEASE VERSION 1.2 -- 15th December 2019

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

#include "guru.h"

#include <chrono>
#include <csignal>
#include <ctime>
#include <fstream>
#include <sstream>

#ifdef GURU_USING_CONSOLE
#include <cstdio>
#endif

#ifdef GURU_USING_CURSES
#include <curses.h>
#include <panel.h>
#endif

#define CASCADE_THRESHOLD		20	// The amount cascade_count can reach within CASCADE_TIMEOUT seconds before it triggers an abort screen.
#define CASCADE_TIMEOUT			30	// The number of seconds without an error to reset the cascade timer.
#define CASCADE_WEIGHT_CRITICAL	4	// The amount a critical type log entry will add to the cascade timer.
#define CASCADE_WEIGHT_ERROR	2	// The amount an error type log entry will add to the cascade timer.
#define CASCADE_WEIGHT_WARNING	1	// The amount a warning type log entry will add to the cascade timer.
#define COLOUR_PAIR_RED			2	// If using Curses, set this to the colour pair number which is red on a black background.
#define FILENAME_LOG			"log.txt"	// The default name of the log file. Another filename can be specified with open_syslog().

// Stack trace system.
std::stack<const char*>	StackTrace::funcs;
StackTrace::StackTrace(const char *func) { funcs.push(func); }
StackTrace::~StackTrace() { if (!funcs.empty()) funcs.pop(); }

namespace guru
{

unsigned int	cascade_count = 0;		// Keeps track of rapidly-occurring, non-fatal error messages.
bool			cascade_failure = false;	// Is a cascade failure in progress?
std::chrono::time_point<std::chrono::system_clock> cascade_timer;	// Timer to check the speed of non-halting Guru warnings, to prevent cascade locks.
bool			dead_already = false;	// Have we already died? Is this crash within the Guru subsystem?
bool			fully_active = false;	// Is the Guru system fully activated yet?
std::string		last_log_message;		// Records the last log message, to avoid spamming the log with repeats.
std::string		message;				// The error message.
std::ofstream	syslog;					// The system log file.


// Like assert(), but calls a Guru halt() if the condition is false.
void affirm(int condition, std::string error)
{
	stack_trace();
	if (!condition) guru::halt(error);
}

// Closes the Guru log file.
void close_syslog()
{
	stack_trace();
	log("Guru system shutting down.");
	log("The rest is silence.");
	syslog.close();
}

// Tells Guru whether or not the console is initialized and can handle rendering error messages.
// This is only necessary for Curses, but will have no adverse effects on non-Curses builds.
void console_ready(bool ready)
{
	fully_active = ready;
}

// Guru meditation error.
void halt(std::string error)
{
	stack_trace();
	auto guru_itos = [](unsigned int num) -> std::string {
		stack_trace();
		std::stringstream ss;
		ss << num;
		return ss.str();
	};

	log("Software Failure, Halting Execution", GURU_CRITICAL);
	log(error, GURU_CRITICAL);

	if (StackTrace::funcs.size())
	{
		log("Stack trace follows:", GURU_STACK);
		while(true)
		{
			log(guru_itos(StackTrace::funcs.size() - 1) + ": " + StackTrace::funcs.top(), GURU_STACK);
			if (StackTrace::funcs.size() > 1) StackTrace::funcs.pop();
			else break;
		}
	}

#ifdef GURU_USING_CURSES
	if (!fully_active) exit(1);
#ifdef PDCURSES
	PDC_set_blink(true);
#endif
#endif

	if (dead_already)
	{
		log("Detected cleanup in process, attempting to die peacefully.", GURU_WARN);
		exit(1);
	}

	message = error;
#ifdef GURU_USING_CONSOLE
	printf("Software Failure, Halting Execution\n");
	printf((message + "\n").c_str());
	exit(1);
#endif
#ifdef GURU_USING_CURSES
	if (message.size() > 39) message = message.substr(0, 39);
	bool redraw = true;
	WINDOW* guru_window = newwin(7, 41, 9, 20);
	PANEL* guru_panel = new_panel(guru_window);
	while(true)
	{
		log("loop");
		if (redraw)
		{
			redraw = false;
#ifdef PDCURSES
			// Workaround to deal with PDCurses' lack of an inbuilt SIGWINCH handler.
			if (is_termresized())
			{
				resize_term(0, 0);
				if (getmaxx(stdscr) < 80 || getmaxy(stdscr) < 24) resize_term(24, 80);
			}
#endif
			curs_set(0);
			wclear(guru_window);
			wattron(guru_window, COLOR_PAIR(COLOUR_PAIR_RED) | A_BOLD | A_BLINK);
			box(guru_window, 0, 0);
			wattroff(guru_window, A_BLINK);
			mvwaddstr(guru_window, 2, 3, "Software Failure, Halting Execution");
			mvwaddstr(guru_window, 4, 20 - (message.size() / 2), message.c_str());
			wattroff(guru_window, COLOR_PAIR(COLOUR_PAIR_RED) | A_BOLD);
			update_panels();
			refresh();
		}
		int key = getch();
		if (key == KEY_RESIZE) redraw = true;
		else if (key == 27)
		{
			curs_set(1);
			echo();
			del_panel(guru_panel);
			delwin(guru_window);
			endwin();
			close_syslog();
			exit(1);
		}
	}
#endif
}

// Catches a segfault or other fatal signal.
void intercept_signal(int sig)
{
	stack_trace();
	std::string sig_type;
	switch(sig)
	{
		case SIGABRT: sig_type = "Software requested abort."; break;
		case SIGFPE: sig_type = "Floating-point exception."; break;
		case SIGILL: sig_type = "Illegal instruction."; break;
		case SIGSEGV: sig_type = "Segmentation fault."; break;
		default: sig_type = "Intercepted unknown signal."; break;
	}

	// Disable the signals for now, to stop a cascade.
	signal(SIGABRT, SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
	signal(SIGILL, SIG_IGN);
	signal(SIGFPE, SIG_IGN);
	halt(sig_type);
}

// Logs a message in the system log file.
void log(std::string msg, int type)
{
	if (!syslog.is_open()) return;
	if (msg == last_log_message) return;
	last_log_message = msg;
	std::string txt_tag;
	switch(type)
	{
		case GURU_INFO: case GURU_STACK: break;
		case GURU_WARN: txt_tag = "[WARN] "; break;
		case GURU_ERROR: txt_tag = "[ERROR] "; break;
		case GURU_CRITICAL: txt_tag = "[CRITICAL] "; break;
	}

	const time_t now = time(nullptr);
	const tm *ptm = localtime(&now);
	char buffer[32];
	strftime(&buffer[0], 32, "%H:%M:%S", ptm);
	std::string time_str = &buffer[0];
	msg = "[" + time_str + "] " + txt_tag + msg;
	syslog << msg << std::endl;
}

// Reports a non-fatal error, which will be logged and displayed in-game but will not halt execution unless it cascades.
void nonfatal(std::string error, int type)
{
	if (cascade_failure) return;
	stack_trace();
	unsigned int cascade_weight = 0;
	switch(type)
	{
		case GURU_WARN: cascade_weight = CASCADE_WEIGHT_WARNING; break;
		case GURU_ERROR: cascade_weight = CASCADE_WEIGHT_ERROR; break;
		case GURU_CRITICAL: cascade_weight = CASCADE_WEIGHT_CRITICAL; break;
		default: nonfatal("Nonfatal error reported with incorrect severity specified.", GURU_WARN); break;
	}

	guru::log(error, type);

	if (cascade_weight)
	{
		std::chrono::duration<float> elapsed_seconds = std::chrono::system_clock::now() - cascade_timer;
		if (elapsed_seconds.count() <= CASCADE_TIMEOUT)
		{
			cascade_count += cascade_weight;
			if (cascade_count > CASCADE_THRESHOLD)
			{
				cascade_failure = true;
				guru::halt("Cascade failure detected!");
			}
		}
		else
		{
			cascade_timer = std::chrono::system_clock::now();
			cascade_count = 0;
		}
	}
}

// Opens the output log for messages.
void open_syslog(std::string filename)
{
	if (!filename.size()) filename = FILENAME_LOG;
	remove(filename.c_str());
	syslog.open(filename.c_str());
	log("Guru error-handling system is online. Hooking signals...");
	if (signal(SIGABRT, intercept_signal) == SIG_ERR) halt("Failed to hook abort signal.");
	if (signal(SIGSEGV, intercept_signal) == SIG_ERR) halt("Failed to hook segfault signal.");
	if (signal(SIGILL, intercept_signal) == SIG_ERR) halt("Failed to hook illegal instruction signal.");
	if (signal(SIGFPE, intercept_signal) == SIG_ERR) halt("Failed to hook floating-point exception signal.");
	cascade_timer = std::chrono::system_clock::now();
}

}	// namespace guru
