/* Copyright (C) 2022 Gareth Francis */

/* Licensed under the Apache License, Version 2.0 (the "License"); */
/* you may not use this file except in compliance with the License. */
/* You may obtain a copy of the License at */

/*     http://www.apache.org/licenses/LICENSE-2.0 */

/* Unless required by applicable law or agreed to in writing, software */
/* distributed under the License is distributed on an "AS IS" BASIS, */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/* See the License for the specific language governing permissions and */
/* limitations under the License. */

#include <sys/signal.h>
#include <SDL2/SDL.h>

#include <atomic>
#include <vector>
#include <map>
#include <string>
#include <iostream>

const int event_data_freq_array_size = 64;

enum class TimedEventType
{
    Keyboard,
    Mouse,
    // TODO: User may have multiple - Need per-device event tracking
    // Joystick,
    // Controller,
};

std::string TimedEventTypeToString(TimedEventType t)
{
    switch(t)
    {
        case TimedEventType::Keyboard:
            return "Keyboard";
        case TimedEventType::Mouse:
            return "Mouse";
    }
    return "Unknown";
}

class EventData
{
    public:
        std::vector<uint32_t> timestamps;
        std::vector<double> freq_array;
        double average_freq = 0.0f;
};

std::atomic<bool> sentinel;
void on_signal_int(int signum)
{
    sentinel = true;
}

void processWindowEvent(SDL_Event& e)
{
    if(e.window.event == SDL_WINDOWEVENT_CLOSE) sentinel = true;
}

void processEvent(TimedEventType t, SDL_Event& e, std::map<TimedEventType, EventData>& events)
{
    auto& v = events[t];

    if(!v.timestamps.empty() && e.common.timestamp == v.timestamps.back())
    {
        // Duplicate, or other non-measurable event
        return;
    }

    if(v.freq_array.empty())
    {
        v.freq_array.push_back(0.f);
    }
    else
    {
        v.freq_array.push_back(1000.0 / static_cast<double>(e.common.timestamp - v.timestamps.back()));
        // std::cout << TimedEventTypeToString(t) << ": " << v.freq_array.back() << std::endl;
    }
    v.timestamps.push_back(e.common.timestamp);

    while(v.freq_array.size() > event_data_freq_array_size)
    {
        v.freq_array.erase(v.freq_array.begin());
        v.timestamps.erase(v.timestamps.begin());
    }
}

void logEventData(std::map<TimedEventType, EventData>& events)
{
    for( auto& t : events )
    {
        auto& e = t.second;
        double avgF = 0.0;
        double maxF = 0.0f;
        if(!e.freq_array.empty())
        {
            for(auto & f : e.freq_array)
            {
                avgF += f;
                maxF = std::max(maxF, f);
            }
            avgF /= e.freq_array.size();
            std::cout << "Type: " << TimedEventTypeToString(t.first) << " Max: " << maxF << "Hz Avg: " << avgF << "Hz" << std::endl;
        }
    }
}

int main(int argc, char** argv)
{
    signal(SIGINT, on_signal_int);
    std::cout << "Checking events from SDL2, Close window or CTRL-C in terminal to exit." << std::endl;

    // <device name, event[]>
    std::map<TimedEventType, EventData> events;

    // Init SDL
    if( SDL_Init(SDL_INIT_VIDEO) != 0 )
    {
        std::cerr << "Failed to init SDL2" << std::endl;
        return EXIT_FAILURE;
    }
    auto window = SDL_CreateWindow(
        "evhz SDL2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        0);
    if(!window)
    {
        std::cerr << "Failed to create window" << std::endl;
        return EXIT_FAILURE;
    }
    auto surface = SDL_GetWindowSurface(window);
    if(!surface)
    {
        std::cerr << "Failed to get window surface" << std::endl;
        return EXIT_FAILURE;
    }
    auto renderer = SDL_CreateSoftwareRenderer(surface);

    auto loopy = 0u;
    SDL_Event e;
    while(!sentinel)
    {
        while(SDL_PollEvent(&e))
        {
            switch(e.type)
            {
                case SDL_WINDOWEVENT:
                    processWindowEvent(e);
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    processEvent(TimedEventType::Keyboard, e, events);
                    break;
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                case SDL_MOUSEWHEEL:
                    processEvent(TimedEventType::Mouse, e, events);
                    break;
                default:
                    break;
                 
/* TODO
    SDL_JOYAXISMOTION  = 0x600,
    SDL_JOYBALLMOTION,         
    SDL_JOYHATMOTION,          
    SDL_JOYBUTTONDOWN,        
    SDL_JOYBUTTONUP,          
*/

    /*
    SDL_CONTROLLERAXISMOTION  = 0x650, 
    SDL_CONTROLLERBUTTONDOWN,         
    SDL_CONTROLLERBUTTONUP,          
    */
            }
        }

        // Render something / log event timings
        SDL_SetRenderDrawColor(renderer, 0x22, 0x22, 0x22, 0xff);
        SDL_RenderClear(renderer);
        SDL_UpdateWindowSurface(window);
        SDL_RenderPresent(renderer);

        ++loopy;
        if(loopy % 200 == 0)
        {
            logEventData(events);
            loopy = 0u;
        }
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}
