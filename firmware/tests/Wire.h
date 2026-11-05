#pragma once
struct WireStub{bool connected=true;void begin(int,int){}void setTimeOut(int){}void beginTransmission(int){}int endTransmission(){return connected?0:4;}};inline WireStub Wire;
