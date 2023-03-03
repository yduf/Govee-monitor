#pragma once

// capture user abort / & terminal hang
// through a global booleen flags.
#include <csignal>
#include <iostream>

extern "C"
void SIGINT_handler(int sig);
extern "C"
void SIGHUP_handler(int sig);


class SignalHandler {
public:
    static
    volatile sig_atomic_t called;                         // signal flags

    sighandler_t prev_SIGINT;
    sighandler_t prev_SIGHUP;

    SignalHandler() {
        called = false;

		prev_SIGINT = std::signal(SIGINT, SIGINT_handler);	// Install CTR-C signal handler
		prev_SIGHUP = std::signal(SIGHUP, SIGHUP_handler);	// Install Hangup signal handler
    }

    ~SignalHandler() {                              	// reinstall default signal(s)
		std::signal(SIGINT, prev_SIGINT);
		std::signal(SIGHUP, prev_SIGHUP);
    }

    static
    void check() {
        if( called) {
            std::cerr << "*** SIGNAL throw\n";
            throw "*** SIGNAL called\n";
        }
    }
};

volatile sig_atomic_t SignalHandler::called = false; 


extern "C"
void SIGINT_handler(int sig) {
    SignalHandler::called = true;
/*
    const char msg[] =  "*** SIGINT: Caught\n";
    write(STDERR_FILENO, msg, sizeof(msg));         // ok, write is signal-safe
*/
}

extern "C" 
void SIGHUP_handler(int sig) {
    SignalHandler::called = true;
/*
    const char msg[] =  "*** SIGHUP: Caught\n";
    write(STDERR_FILENO, msg, sizeof(msg));         // ok, write is signal-safe
*/
}
