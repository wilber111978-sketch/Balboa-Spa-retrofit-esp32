// Host visual mock: executes the same clipped strip painter; no display hardware.
#include "TopsideDisplay.h"
#include <fstream>
#include <sstream>
#include <iostream>
struct SvgCanvas {
 std::ostringstream out;int stripY=0,x=0,y=0,size=1;unsigned color=0xffff;
 void fillScreen(int){}void setTextWrap(bool){}void setTextColor(unsigned c){color=c;}void setTextSize(int n){size=n;}void setCursor(int a,int b){x=a;y=b;}
 void print(const char*s){
  unsigned red=((color>>11)&31)*255/31,green=((color>>5)&63)*255/63,blue=(color&31)*255/31;
  out<<"<text x='"<<x<<"' y='"<<stripY+y+size*7<<"' fill='rgb("<<red<<","<<green<<","<<blue<<")' font-family='monospace' font-size='"<<size*8<<"' textLength='"<<strlen(s)*size*6<<"' lengthAdjust='spacingAndGlyphs'>";
  for(;*s;s++){if(*s=='&')out<<"&amp;";else if(*s=='<')out<<"&lt;";else out<<*s;}out<<"</text>\n";
 }
};
int main(int argc,char**argv){
 if(argc!=2)return 2;
 topside::State s;s.valid=true;s.water=36.8;s.target=38;s.wifi=true;s.pump=1;topside::copy(s.mode,sizeof(s.mode),"Solar Priority");
 topside::Model model;
 for(const char*name:{"normal","adjust","fault","service","invalid","info"}){
  s.fault=0;s.service=false;s.valid=true;topside::Model m;
  if(!strcmp(name,"adjust"))m.acceptedTemperature(100);
  if(!strcmp(name,"fault")){s.fault=8;topside::copy(s.faultId,sizeof(s.faultId),"OVER_TEMP");topside::copy(s.description,sizeof(s.description),"Overtemperature lockout");}
  if(!strcmp(name,"service"))s.service=true;
  if(!strcmp(name,"invalid")){s.valid=false;s.water=NAN;}
  if(!strcmp(name,"info"))m.information(100);
  auto v=m.view(s,100);SvgCanvas c;
  c.out<<"<svg xmlns='http://www.w3.org/2000/svg' width='240' height='240' viewBox='0 0 240 240'><circle cx='120' cy='120' r='120' fill='black'/>\n";
  for(int y=0;y<240;y+=8){c.stripY=y;c.out<<"<defs><clipPath id='s"<<y<<"'><rect x='0' y='"<<y<<"' width='240' height='8'/></clipPath></defs><g clip-path='url(#s"<<y<<")'>\n";topside::paintStrip(c,v,y);c.out<<"</g>\n";}
  c.out<<"</svg>\n";std::ofstream file(std::string(argv[1])+"/"+name+".svg");file<<c.out.str();if(!file)return 3;
 }
 std::cout<<"Six host screen mocks rendered with shared strip painter\n";
}
