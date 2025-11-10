#pragma once

#include <string>
#include <array>
#include <cstdint>

#if defined(__MINGW32__) || defined(_MSC_VER)
#include <d3dx9.h>
#else
struct tagRECT {
    long left;
    long top;
    long right;
    long bottom;
};
using RECT = tagRECT;
#endif


#define DEFAULT_MESSAGE_TIMEOUT ( 3000 )


namespace DllOverlayUi
{

void init();


void enable();

void disable();

void toggle();

bool isEnabled();

bool isDisabled();

std::array<std::string, 3> getText();

int getHeight();

int getNewHeight();

std::array<RECT, 2> getSelector();

std::array<bool, 2> getShouldDrawSelector();

std::array<std::string, 2> getSelectorLine();

bool isTrial();

bool isMapping();

void setTrial();

void setMapping();

bool isToggling();

void updateText();

void updateText ( const std::array<std::string, 3>& text );

void updateSelector ( uint8_t index, int position = 0, const std::string& line = "" );


void showMessage ( const std::string& text, int timeout = DEFAULT_MESSAGE_TIMEOUT );

void updateMessage();

bool isShowingMessage();

#ifndef RELEASE

extern std::string debugText;

extern int debugTextAlign;

#endif

}
