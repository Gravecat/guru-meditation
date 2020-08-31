/* guru.cpp -- Guru error-handling and reporting system.
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

#include "guru.h"

#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdlib>
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

#ifdef GURU_USING_LIBTCOD
#include "libtcod/libtcod.h"
#endif

namespace guru
{

#define CASCADE_THRESHOLD		20	// The amount cascade_count can reach within CASCADE_TIMEOUT seconds before it triggers an abort screen.
#define CASCADE_TIMEOUT			30	// The number of seconds without an error to reset the cascade timer.
#define CASCADE_WEIGHT_CRITICAL	4	// The amount a critical type log entry will add to the cascade timer.
#define CASCADE_WEIGHT_ERROR	2	// The amount an error type log entry will add to the cascade timer.
#define CASCADE_WEIGHT_WARNING	1	// The amount a warning type log entry will add to the cascade timer.
#define COLOUR_PAIR_RED			2	// If using Curses, set this to the colour pair number which is red on a black background.
#define FILENAME_LOG			"log.txt"	// The default name of the log file. Another filename can be specified with open_syslog().

#ifdef GURU_USING_STACK_TRACE
// Stack trace system.
std::stack<const char*>	StackTrace::funcs;
StackTrace::StackTrace(const char *func) { funcs.push(func); }
StackTrace::~StackTrace() { if (!funcs.empty()) funcs.pop(); }
#endif

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
#ifdef GURU_USING_STACK_TRACE
	stack_trace();
#endif
	if (!condition) guru::halt(error);
}

// Closes the Guru log file.
void close_syslog()
{
#ifdef GURU_USING_STACK_TRACE
	stack_trace();
#endif
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
void halt(std::string error, ...)
{
#ifdef GURU_USING_STACK_TRACE
	stack_trace();
#endif

	// Construct the error string, if needed.
	va_list ap;
	char *buffer = new char[error.size() + 1024];
	va_start(ap, error);
	vsprintf(buffer, error.c_str(), ap);
	va_end(ap);
	error = std::string(buffer);
	delete[] buffer;

	log("Software Failure, Halting Execution", GURU_CRITICAL);
	log(error, GURU_CRITICAL);

#ifdef GURU_USING_STACK_TRACE
	if (StackTrace::funcs.size())
	{
		log("Stack trace follows:", GURU_STACK);
		while(true)
		{
			log(std::to_string(StackTrace::funcs.size() - 1) + ": " + StackTrace::funcs.top(), GURU_STACK);
			if (StackTrace::funcs.size() > 1) StackTrace::funcs.pop();
			else break;
		}
	}
#endif

#ifdef GURU_USING_CURSES
	if (!fully_active) exit(EXIT_FAILURE);
#ifdef PDCURSES
	PDC_set_blink(true);
#endif
#endif

	if (dead_already)
	{
		log("Detected cleanup in process, attempting to die peacefully.", GURU_WARN);
		exit(EXIT_FAILURE);
	}

	message = error;
#ifdef GURU_USING_CONSOLE
	printf("Software Failure, Halting Execution\n");
	printf((message + "\n").c_str());
	exit(EXIT_FAILURE);
#endif
#ifdef GURU_USING_CURSES
	if (message.size() > 39) message = message.substr(0, 39);
	bool redraw = true;
	WINDOW* guru_window = newwin(7, 41, 9, 20);
	PANEL* guru_panel = new_panel(guru_window);
	while(true)
	{
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
			exit(EXIT_FAILURE);
		}
	}
#endif

#ifdef GURU_USING_LIBTCOD
	TCODConsole guru_con(41, 7);
	guru_con.setDefaultForeground(TCODColor::red);
	bool flash_state = false;
	int redraw_cycle = 50;
	TCOD_key_t keypress;

	auto redraw_guru_con = [&guru_con, &flash_state]()
	{
		guru_con.clear();
		flash_state = !flash_state;
		if (flash_state) guru_con.printFrame(0, 0, 41, 7, true, TCOD_BKGND_DEFAULT);
		guru_con.printf(3, 2, "Software Failure: Halting Execution");
		guru_con.printf(20 - (message.size() / 2), 4, message.c_str());
	};

	while (!TCODConsole::isWindowClosed() && keypress.vk != TCODK_ESCAPE)
	{
		if (++redraw_cycle >= 50)
		{
			redraw_cycle = 0;
			redraw_guru_con();
		}
		TCODConsole::blit(&guru_con, 0, 0, 41, 7, TCODConsole::root, (TCODConsole::root->getWidth() / 2) - 20, (TCODConsole::root->getHeight() / 2) - 3);
		TCODConsole::flush();
		TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS, &keypress, nullptr);
		TCODSystem::sleepMilli(10);
	}
#endif

#ifndef GURU_USING_CONSOLE
#ifndef GURU_USING_CURSES
	exit(EXIT_FAILURE);
#endif
#endif
}

// As above, but with an exception instead of a string.
void halt(std::exception &e)
{
#ifdef GURU_USING_STACK_TRACE
	stack_trace();
#endif
	guru::halt(e.what());
}

// Catches a segfault or other fatal signal.
void intercept_signal(int sig)
{
#ifdef GURU_USING_STACK_TRACE
	stack_trace();
#endif
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
void log(std::string msg, int type, ...)
{
	if (!syslog.is_open()) return;
	if (msg == last_log_message) return;

	// Construct the log string, if needed.
	va_list ap;
	char *buffer = new char[msg.size() + 1024];
	va_start(ap, type);
	vsprintf(buffer, msg.c_str(), ap);
	va_end(ap);
	msg = std::string(buffer);
	delete[] buffer;

	last_log_message = msg;
	std::string txt_tag;
	switch(type)
	{
		case GURU_INFO:
#ifdef GURU_USING_STACK_TRACE
		case GURU_STACK:
#endif
			break;
		case GURU_WARN: txt_tag = "[WARN] "; break;
		case GURU_ERROR: txt_tag = "[ERROR] "; break;
		case GURU_CRITICAL: txt_tag = "[CRITICAL] "; break;
	}

	buffer = new char[32];
	const time_t now = time(nullptr);
	const tm *ptm = localtime(&now);
	strftime(&buffer[0], 32, "%H:%M:%S", ptm);
	std::string time_str = &buffer[0];
	msg = "[" + time_str + "] " + txt_tag + msg;
	syslog << msg << std::endl;
	delete[] buffer;
}

// Reports a non-fatal error, which will be logged but will not halt execution unless it cascades.
void nonfatal(std::string error, int type, ...)
{
	if (cascade_failure) return;
#ifdef GURU_USING_STACK_TRACE
	stack_trace();
#endif

	// Construct the error string, if needed.
	va_list ap;
	char *buffer = new char[error.size() + 1024];
	va_start(ap, type);
	vsprintf(buffer, error.c_str(), ap);
	va_end(ap);
	error = std::string(buffer);
	delete[] buffer;

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
