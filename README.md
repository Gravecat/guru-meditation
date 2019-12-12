# Guru Meditation

Guru Meditation is a simple and lightweight system that can be used for error-tracking, logging, catching signals (segfault, abort, etc.) and a simple, system-agnostic stack-trace. It was primarily developed for my own personal projects, but anyone is welcome to use it under the license terms below.

Add the source files to your C++ project, and uncomment the appropriate line in guru.h if you're using PDCurses/NCurses, or a non-Curses project with console output. Initialize the system with guru::open_syslog(), and if using Curses, call guru::console_ready(true) when your Curses system and window are all set up and running. When shutting down normally, call guru::close_syslog().

The system will automatically catch segfault, abort, illegal instruction and floating-point exception signals. For other errors, simply call guru::halt() with an appropriate error message and severity level. For non-error logging, use guru::log(), and for non-fatal errors, call guru::nonfatal() in the same manner as halt().

For the stack-trace system, simply put stack_trace(); at the start of each function in your code, and guru::halt() will automatically generate a stack-trace when an error occurs. This works even if the code is compiled with stripped symbols.


## MIT License

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
