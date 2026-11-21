#pragma once
namespace topside {
// Matches docs/TOPSIDE_V08_HARDWARE.md. No relay pins appear here.
static constexpr int Sclk=10,Mosi=11,Cs=12,Dc=13,Reset=14,Backlight=15;
static constexpr int ButtonPins[]={4,5,6,7}; // Jets, Light, Up, Down; active LOW
static constexpr unsigned DebounceMs=40,LongPressMs=800,RepeatMs=250,StuckMs=10000;
static constexpr unsigned PreviewMs=5000,RefreshMs=200;
}
// Screen blackout: display and backlight off after this long without a button press.
// Faults and Service keep the screen on. 0 disables the blackout.
#ifndef SPA_TOPSIDE_SCREEN_TIMEOUT_MS
#define SPA_TOPSIDE_SCREEN_TIMEOUT_MS 3600000UL // 1 hour
#endif
// Display rotation in 90-degree steps, to suit how the screen is mounted:
// 0 = header pins at the top, 1 = 90 deg clockwise, 2 = 180 deg, 3 = 270 deg.
#ifndef SPA_TOPSIDE_ROTATION
#define SPA_TOPSIDE_ROTATION 3  // topside panel is mounted turned 270 deg
#endif
#ifndef SPA_TOPSIDE_HARDWARE
#define SPA_TOPSIDE_HARDWARE (!SPA_BENCH_DEMO)
#endif
#ifndef SPA_TOPSIDE_BACKLIGHT_VERIFIED
#define SPA_TOPSIDE_BACKLIGHT_VERIFIED 0
#endif
