#pragma once
#include <string>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <map>
#include <functional>
#include <algorithm>
using std::isfinite; using std::isnan;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
inline uint32_t fakeMs=100;
inline int pins[64]={};
inline uint32_t millis(){return fakeMs;}
inline void delay(uint32_t x){fakeMs+=x;}
inline void digitalWrite(int p,int v){if(p>=0) pins[p]=v;}
inline int digitalRead(int p){return p>=0?pins[p]:0;}
inline void pinMode(int,int){}
// V0.9 onboard ADC mocks (millivolts per pin)
#define ADC_11db 3
inline uint32_t fakeMv[64]={};
inline uint32_t analogReadMilliVolts(int p){return p>=0?fakeMv[p]:0;}
inline void analogSetAttenuation(int){}
inline void analogReadResolution(int){}
template<class T> T constrain(T v,T a,T b){return std::min(std::max(v,a),b);}
class String {
public:
 std::string s;
 String(){} String(const char*x):s(x){} String(std::string x):s(x){} String(char x):s(1,x){}
 template<class T> String(T x):s(std::to_string(x)){}
 String(float x,int n){char b[64];snprintf(b,sizeof b,"%.*f",n,x);s=b;}
 const char*c_str()const{return s.c_str();} size_t length()const{return s.size();}
 char operator[](int i)const{return s[i];}
 void trim(){auto a=s.find_first_not_of(" \r\n\t");if(a==std::string::npos){s.clear();return;}s=s.substr(a,s.find_last_not_of(" \r\n\t")-a+1);}
 void toLowerCase(){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});}
 int indexOf(char c)const{auto p=s.find(c);return p==std::string::npos?-1:int(p);}
 String substring(int a)const{return s.substr(a);} String substring(int a,int b)const{return s.substr(a,b-a);}
 float toFloat()const{return strtof(s.c_str(),nullptr);} int toInt()const{return atoi(s.c_str());}
 bool startsWith(const char*x)const{return s.rfind(x,0)==0;}
 String& operator+=(const String&x){s+=x.s;return *this;}
 friend String operator+(String a,const String&b){return a+=b;}
 friend bool operator==(const String&a,const String&b){return a.s==b.s;}
 friend bool operator!=(const String&a,const String&b){return a.s!=b.s;}
};
struct SerialStub{void begin(int){} int available(){return 0;} char read(){return 0;} void println(){} template<class T>void println(T){} void printf(const char*,...){} };inline SerialStub Serial;
inline bool fakeClockValid=false; inline int fakeMinute=660,fakeDay=1;
inline bool getLocalTime(tm*t,uint32_t){if(!fakeClockValid)return false;*t={};t->tm_year=126;t->tm_yday=fakeDay;t->tm_hour=fakeMinute/60;t->tm_min=fakeMinute%60;return true;}
inline void configTzTime(const char*,const char*,const char*){}
