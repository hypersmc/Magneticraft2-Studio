// ZIP STORE writer without dependencies (no compression, Zip64, or filenames > 65535 bytes).
const u16=(view,n,v)=>view.setUint16(n,v,true), u32=(view,n,v)=>view.setUint32(n,v>>>0,true);
const encoder=new TextEncoder();
const table=Uint32Array.from({length:256},(_,n)=>{let c=n;for(let k=0;k<8;k++) c=c&1 ? 0xedb88320^(c>>>1) : c>>>1;return c>>>0;});
export const crc32=data=>{let c=0xffffffff;for(let i=0;i<data.length;i++) c=table[(c^data[i])&255]^(c>>>8);return (c^0xffffffff)>>>0;};
export function createZip(files){
  const pieces=[],directory=[]; let offset=0;
  for(const [filename,content] of Object.entries(files)) {
    const name=encoder.encode(filename), data=typeof content==='string' ? encoder.encode(content) : content;
    if(name.length>65535) throw Error('ZIP path too long');
    const crc=crc32(data),h=new Uint8Array(30+name.length),v=new DataView(h.buffer);
    u32(v,0,0x04034b50);u16(v,4,20);u16(v,6,0x0800);u16(v,8,0);u32(v,14,crc);u32(v,18,data.length);u32(v,22,data.length);u16(v,26,name.length);h.set(name,30);
    pieces.push(h,data);
    const d=new Uint8Array(46+name.length),dv=new DataView(d.buffer);
    u32(dv,0,0x02014b50);u16(dv,4,20);u16(dv,6,20);u16(dv,8,0x0800);u32(dv,16,crc);u32(dv,20,data.length);u32(dv,24,data.length);u16(dv,28,name.length);u32(dv,42,offset);d.set(name,46);
    directory.push(d);offset+=h.length+data.length;
  }
  const dirLength=directory.reduce((sum,d)=>sum+d.length,0),end=new Uint8Array(22),ev=new DataView(end.buffer);
  u32(ev,0,0x06054b50);u16(ev,8,directory.length);u16(ev,10,directory.length);u32(ev,12,dirLength);u32(ev,16,offset);
  const output=new Uint8Array(offset+dirLength+end.length);let ptr=0;
  for(const p of [...pieces,...directory,end]){output.set(p,ptr);ptr+=p.length;}
  return output;
}
