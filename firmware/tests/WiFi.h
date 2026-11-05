#pragma once
#include "Arduino.h"
#include <memory>
#include <deque>
#define WIFI_STA 1
#define WL_CONNECTED 3
namespace MockSockets {
struct Socket {std::string input,output;bool active=true,blockWrite=false;};
inline std::map<int,std::shared_ptr<Socket>> sockets;
inline std::deque<int> pending;
inline int next=3;
inline int connect(std::string input=""){int id=next++;sockets[id]=std::make_shared<Socket>();sockets[id]->input=input;pending.push_back(id);return id;}
}
class WiFiClient {int id=-1;public:WiFiClient(){}WiFiClient(int x):id(x){}operator bool()const{return id>=0&&MockSockets::sockets[id]->active;}int fd()const{return id;}void stop(){if(id>=0)MockSockets::sockets[id]->active=false;}};
class WiFiServer {public:explicit WiFiServer(uint16_t){}void begin(){}WiFiClient accept(){if(MockSockets::pending.empty())return {};int id=MockSockets::pending.front();MockSockets::pending.pop_front();return WiFiClient(id);}};
struct WiFiStub{int state=3;int reconnects=0;void mode(int){}void setAutoReconnect(bool){}void begin(const char*,const char*){}int status(){return state;}void reconnect(){reconnects++;}};inline WiFiStub WiFi;
