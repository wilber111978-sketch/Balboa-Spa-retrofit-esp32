#pragma once
#include "Arduino.h"
#include <vector>
class Preferences{public:
 static inline std::map<std::string,double> db;
 static inline std::map<std::string,std::vector<uint8_t>> blobs;
 static inline bool failBegin=false,failWrite=false,shortWrite=false,badReadback=false;
 bool begin(const char*,bool=false){return !failBegin;} void end(){}
 bool isKey(const char*k){return db.count(k)||blobs.count(k);}
 float getFloat(const char*k,float d){return db.count(k)?db[k]:d;}
 uint32_t getUInt(const char*k,uint32_t d){return db.count(k)?uint32_t(db[k]):d;}
 uint8_t getUChar(const char*k,uint8_t d){return db.count(k)?uint8_t(db[k]):d;}
 size_t putFloat(const char*k,float v){if(failWrite)return 0;db[k]=v;return 4;}
 size_t putUInt(const char*k,uint32_t v){if(failWrite)return 0;db[k]=v;return 4;}
 size_t putUChar(const char*k,uint8_t v){if(failWrite)return 0;db[k]=v;return 1;}
 size_t putBytes(const char*k,const void*p,size_t n){if(failWrite)return 0;if(shortWrite)--n;auto b=(const uint8_t*)p;blobs[k]=std::vector<uint8_t>(b,b+n);return n;}
 size_t getBytesLength(const char*k){return blobs.count(k)?blobs[k].size():0;}
 size_t getBytes(const char*k,void*p,size_t n){if(!blobs.count(k))return 0;n=std::min(n,blobs[k].size());memcpy(p,blobs[k].data(),n);if(badReadback&&n)((uint8_t*)p)[0]^=1;return n;}
};
