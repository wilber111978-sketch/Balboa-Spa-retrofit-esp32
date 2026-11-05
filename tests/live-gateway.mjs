// Native host-test gateway only. Not a production server or ESP32 deployment.
import http from 'node:http';import {spawn} from 'node:child_process';import readline from 'node:readline';
export async function startGateway({port=8090,executable=new URL('../firmware/live-tests/bridge',import.meta.url).pathname}={}){
 const child=spawn(executable,[],{stdio:['pipe','pipe','inherit']});const pending=[];
 const lines=readline.createInterface({input:child.stdout});
 lines.on('line',line=>{const job=pending.shift();try{job?.resolve(JSON.parse(line));}catch(error){job?.reject(error);}});
 child.on('exit',code=>{for(const job of pending.splice(0))job.reject(new Error(`Native adapter exited (${code})`));});
 const call=value=>new Promise((resolve,reject)=>{pending.push({resolve,reject});child.stdin.write(JSON.stringify(value)+'\n');});
 let drop=false;
 const server=http.createServer(async(req,res)=>{try{
  if(req.url==='/__test/drop'){drop=!drop;res.end('ok');return;}
  if(req.url?.startsWith('/__test/inject/')){await call({inject:decodeURIComponent(req.url.split('/').pop())});res.end('ok');return;}
  if(drop&&req.url?.startsWith('/api/')){res.writeHead(503);res.end('offline test');return;}
  let body='';for await(const chunk of req){body+=chunk;if(body.length>384){res.writeHead(413);res.end();return;}}
  const result=await call({path:req.url,method:req.method,body,authorization:req.headers.authorization||''});
  const raw=Buffer.from(result.response,'base64'),at=raw.indexOf('\r\n\r\n');const headers=raw.subarray(0,at).toString().split('\r\n');const code=Number(headers.shift().split(' ')[1]);const mapped={};for(const h of headers){const colon=h.indexOf(':');mapped[h.slice(0,colon)]=h.slice(colon+1).trim();}res.writeHead(code,mapped);res.end(raw.subarray(at+4));
 }catch(error){res.writeHead(500);res.end(error.message);}});
 await new Promise(resolve=>server.listen(port,'127.0.0.1',resolve));const ticker=setInterval(()=>call({tick:100}).catch(()=>{}),100);
 return {url:`http://127.0.0.1:${port}/`,call,close:async()=>{clearInterval(ticker);await new Promise(resolve=>server.close(resolve));child.stdin.end();lines.close();child.kill();}};
}
