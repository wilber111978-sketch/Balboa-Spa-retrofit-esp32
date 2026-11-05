#pragma once
#include "WiFi.h"
#define MSG_DONTWAIT 1
inline int recv(int id,void*buffer,size_t n,int){auto s=MockSockets::sockets[id];if(!s->active)return 0;if(s->input.empty())return -1;n=std::min(n,s->input.size());memcpy(buffer,s->input.data(),n);s->input.erase(0,n);return (int)n;}
inline int send(int id,const void*buffer,size_t n,int){auto s=MockSockets::sockets[id];if(!s->active||s->blockWrite)return -1;s->output.append((const char*)buffer,n);return (int)n;}
