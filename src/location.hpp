#pragma once


struct Location {
    size_t line;
    size_t col;

    Location(): line(1), col(1) {}
    auto reset()->void { Location();}
    auto advance(const char c) -> void {
        if (c == '\n') {
            col = 1;
            line++;
        }
        else col++;
    }
};