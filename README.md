# CLIBuilder
Build Cisco like ( or even better ) Command Line Interface 

**Dependency :**
Make sure you have** ncurses** library installed on your system.
To install ncurses, **sudo apt-get install ncurses-dev**


**Compile :**
Just run **make** in top level dir.

It will create a executable testapp.exe which you can use to understand the CLI library usage.
Library file is created** libclibuilder.a** which you can integrate with your C/C++ applications.
HDr file libcli.h is exposed to be used by appication developer. Pls Note : use **printf** only to print
the output on the screen in your application code. Dont use cout !! 

Highlighting features :
=============================
1. work with C and C++ applications.
2. Developed on linux, may work on windows also but not tested.
3. Auto-completions.
4. CLI history is maintained.
5. '?' Display next set of possible options
6. '.' show command completions
7. use 'show help' command to know more after running the executable.
