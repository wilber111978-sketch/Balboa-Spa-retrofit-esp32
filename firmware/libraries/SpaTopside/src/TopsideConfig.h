#pragma once
namespace topside {
// Matches docs/TOPSIDE_V08_HARDWARE.md. No relay pins appear here.
static constexpr int Sclk=10,Mosi=11,Cs=12,Dc=13,Reset=14,Backlight=15;
static constexpr int ButtonPins[]={4,5,6,7}; // Jets, Light, Up, Down; active LOW
static constexpr unsigned DebounceMs=40,LongPressMs=800,RepeatMs=250,StuckMs=10000;
static constexpr unsigned PreviewMs=5000,RefreshMs=200;
}
#ifndef SPA_TOPSIDE_HARDWARE
#define SPA_TOPSIDE_HARDWARE (!SPA_BENCH_DEMO)
#endif
#ifndef SPA_TOPSIDE_BACKLIGHT_VERIFIED
#define SPA_TOPSIDE_BACKLIGHT_VERIFIED 0
#endif
