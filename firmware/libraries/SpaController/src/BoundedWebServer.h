#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <sys/socket.h>
#include <functional>
#include <map>
#include <string>
#include <algorithm>

// Deliberately small HTTP/1.x subset. One connection; no keepalive, chunked
// bodies, multipart uploads or blocking Stream/NetworkClient read/write APIs.
static constexpr int HTTP_GET = 0, HTTP_POST = 1;
class BoundedWebServer {
public:
    static constexpr size_t RX_BUDGET = 128, TX_BUDGET = 128;
    static constexpr size_t HEADER_LIMIT = 1024, BODY_LIMIT = 384, RESPONSE_LIMIT = 4096;
    static constexpr uint32_t REQUEST_MS = 1500, RESPONSE_MS = 1500;
    int response = 0;
    uint32_t expired = 0, rejected = 0;
    size_t lastRxBytes = 0, lastTxBytes = 0;
    explicit BoundedWebServer(uint16_t port): server(port) {}
    void on(const char *path, int method, std::function<void()> handler) { routes[path] = {method,handler}; }
    void begin() { server.begin(); }
    bool hasArg(const char *name) const { return !strcmp(name,"cmd") && hasCommand; }
    String arg(const char *) const { return String(command.c_str()); }
    bool authenticate(const char *user, const char *pass) const {
        std::string credentials = std::string(user) + ":" + pass;
        if (credentials.size() > 192) return false;
        std::string expected = "Basic " + base64(credentials);
        if (authorization.size() != expected.size()) return false;
        unsigned different = 0;
        for (size_t i=0;i<expected.size();++i) different |= (unsigned char)expected[i] ^ (unsigned char)authorization[i];
        return different == 0;
    }
    void requestAuthentication() { send(401,"text/plain","Authentication required"); }
    void send(int code, const char *type, const String &body) {
        response = code;
        std::string text = body.c_str();
        if (text.size() > RESPONSE_LIMIT) { response=503; text="Response too large"; }
        output = "HTTP/1.1 " + std::to_string(response) + " Response\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Type: " + type +
            "\r\nContent-Length: " + std::to_string(text.size()) + "\r\n";
        if (response==401) output += "WWW-Authenticate: Basic realm=\"spa\"\r\n";
        output += "\r\n" + text;
        sent = 0; responding = true; phaseStarted = millis();
    }
    void handleClient() {
        lastRxBytes = lastTxBytes = 0;
        if (!active) {
            client = server.accept();
            if (!client) return;
            active = true; phaseStarted = millis();
        }
        if (millis()-phaseStarted >= (responding ? RESPONSE_MS : REQUEST_MS)) { ++expired; close(); return; }
        if (!responding) {
            char bytes[RX_BUDGET];
            int received = ::recv(client.fd(), bytes, sizeof(bytes), MSG_DONTWAIT);
            if (received==0) { close(); return; }
            if (received>0) {
                lastRxBytes = (size_t)received;
                input.append(bytes, (size_t)received);
                if (input.size() > HEADER_LIMIT+BODY_LIMIT) { fail(413); }
                else parse();
            }
        }
        if (responding) {
            size_t bytes = std::min(TX_BUDGET, output.size()-sent);
            int written = ::send(client.fd(), output.data()+sent, bytes, MSG_DONTWAIT);
            if (written>0) { lastTxBytes=(size_t)written; sent+=(size_t)written; }
            if (sent==output.size()) close();
        }
    }
private:
    struct Route { int method; std::function<void()> handler; };
    WiFiServer server;
    WiFiClient client;
    std::map<std::string,Route> routes;
    bool active=false, responding=false, hasCommand=false;
    uint32_t phaseStarted=0;
    size_t sent=0;
    std::string input,output,command,authorization;
    void close() {
        client.stop(); active=responding=hasCommand=false;
        input.clear();output.clear();command.clear();authorization.clear(); sent=0;
    }
    void fail(int code) { ++rejected; send(code,"text/plain","Invalid or unsupported request"); }
    static std::string trim(std::string s) {
        size_t a=s.find_first_not_of(" \t"),b=s.find_last_not_of(" \t");
        return a==std::string::npos ? "" : s.substr(a,b-a+1);
    }
    static int hex(char c) { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1; }
    static bool decode(const std::string &s,std::string &out) {
        out.clear();
        for(size_t i=0;i<s.size();++i) {
            char c=s[i];
            if(c=='%') { if(i+2>=s.size()||hex(s[i+1])<0||hex(s[i+2])<0)return false;c=(char)((hex(s[i+1])<<4)|hex(s[i+2]));i+=2; }
            else if(c=='+')c=' ';
            if((unsigned char)c<32 || (unsigned char)c>126)return false;
            out+=c;
            if(out.size()>96)return false;
        }
        return true;
    }
    void parse() {
        size_t end=input.find("\r\n\r\n");
        if(end==std::string::npos) { if(input.size()>HEADER_LIMIT)fail(431);return; }
        if(end+4>HEADER_LIMIT) { fail(431);return; }
        size_t lineEnd=input.find("\r\n");
        if(lineEnd>256) { fail(400);return; }
        std::string line=input.substr(0,lineEnd);
        size_t a=line.find(' '),b=line.find(' ',a==std::string::npos?0:a+1);
        if(a==std::string::npos||b==std::string::npos||(line.substr(b+1)!="HTTP/1.1"&&line.substr(b+1)!="HTTP/1.0")) { fail(400);return; }
        std::string verb=line.substr(0,a), path=line.substr(a+1,b-a-1);
        if(verb!="GET"&&verb!="POST") { fail(405);return; }
        size_t length=0; bool lengthSeen=false; std::string contentType;
        authorization.clear();
        for(size_t pos=lineEnd+2;pos<end;) {
            size_t next=input.find("\r\n",pos),colon=input.find(':',pos);
            if(next==std::string::npos||colon==std::string::npos||colon>=next) { fail(400);return; }
            std::string key=input.substr(pos,colon-pos),value=trim(input.substr(colon+1,next-colon-1));
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return (char)tolower(c);});
            if(key=="content-length") {
                if(lengthSeen||value.empty()||value.size()>3) { fail(400);return; }
                lengthSeen=true;
                for(char c:value) { if(c<'0'||c>'9') {fail(400);return;}length=length*10+c-'0'; }
                if(length>BODY_LIMIT) { fail(413);return; }
            } else if(key=="transfer-encoding") { fail(400);return; }
            else if(key=="authorization") { if(!authorization.empty()){fail(400);return;}authorization=value; }
            else if(key=="content-type")contentType=value;
            pos=next+2;
        }
        if(verb=="POST"&&(!lengthSeen||contentType!="application/x-www-form-urlencoded")) { fail(400);return; }
        if(input.size()<end+4+length)return;
        // Reject pipelining/trailing bytes before executing a command.
        if(input.size()!=end+4+length) { fail(400);return; }
        hasCommand=false; command.clear();
        if(verb=="POST") {
            std::string body=input.substr(end+4,length);
            // Only one explicit cmd field is supported, avoiding ambiguous duplicates.
            if(!body.empty()) {
                if(body.rfind("cmd=",0)!=0||body.find('&')!=std::string::npos||!decode(body.substr(4),command)) {fail(400);return;}
                hasCommand=true;
            }
        }
        auto route=routes.find(path);
        if(route==routes.end()) {fail(404);return;}
        if(route->second.method!=(verb=="GET"?HTTP_GET:HTTP_POST)) {fail(405);return;}
        route->second.handler(); // at most one command; no IO waits in handler
        if(!responding)send(500,"text/plain","Handler produced no response");
    }
    static std::string base64(const std::string &in) {
        static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out; uint32_t value=0;int bits=-6;
        for(unsigned char c:in) {value=(value<<8)|c;bits+=8;while(bits>=0){out+=alphabet[(value>>bits)&63];bits-=6;}}
        if(bits>-6)out+=alphabet[((value<<8)>>(bits+8))&63];
        while(out.size()%4)out+='=';
        return out;
    }
};
