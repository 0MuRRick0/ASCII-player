#pragma once
#include <sys/ioctl.h>
#include <unistd.h>

struct TerminalSize {
    int cols = 80, rows = 24;
    void update() {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            cols = w.ws_col > 0 ? w.ws_col : 80;
            rows = w.ws_row > 0 ? w.ws_row : 24;
        }
    }
};