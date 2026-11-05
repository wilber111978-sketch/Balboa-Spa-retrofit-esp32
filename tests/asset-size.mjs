import {readFile,readdir} from 'node:fs/promises';
import {gzipSync} from 'node:zlib';
let total=0,gzip=0;
for(const file of await readdir(new URL('../firmware/data/',import.meta.url))){const data=await readFile(new URL('../firmware/data/'+file,import.meta.url));total+=data.length;gzip+=gzipSync(data).length;}
console.log(`Static payload: ${total} bytes raw; ${gzip} bytes gzip (no runtime dependencies).`);
if(total>100*1024)throw Error('Static app exceeds 100 KiB budget.');
