import { clamp, cellKey, coordsOf, materialById, materialForBlock } from './core.mjs';
const FACES = [
 {n:'north',points:[0,1,2,3],normal:[0,0,-1],shade:.76,neighbor:[0,0,-1]},
 {n:'south',points:[5,4,7,6],normal:[0,0,1],shade:.91,neighbor:[0,0,1]},
 {n:'west',points:[4,0,3,7],normal:[-1,0,0],shade:.70,neighbor:[-1,0,0]},
 {n:'east',points:[1,5,6,2],normal:[1,0,0],shade:.79,neighbor:[1,0,0]},
 {n:'up',points:[3,2,6,7],normal:[0,1,0],shade:1.14,neighbor:[0,1,0]},
 {n:'down',points:[4,5,1,0],normal:[0,-1,0],shade:.60,neighbor:[0,-1,0]},
];
function rgb(hex){const v=String(hex||'#7e8994').replace('#','');const h=v.length===3?v.split('').map(x=>x+x).join(''):v;return [parseInt(h.slice(0,2),16)||0,parseInt(h.slice(2,4),16)||0,parseInt(h.slice(4,6),16)||0];}
function shadeColor(hex,scale,alpha=1){const c=rgb(hex);return `rgba(${c.map(v=>clamp(Math.round(v*scale),0,255)).join(',')},${alpha})`;}
function boxCorners(from,to){const [a,b,c]=from,[x,y,z]=to;return [[a,b,c],[x,b,c],[x,y,c],[a,y,c],[a,b,z],[x,b,z],[x,y,z],[a,y,z]];}
const inside=(point,polygon)=>{let hit=false;for(let i=0,j=polygon.length-1;i<polygon.length;j=i++) {const a=polygon[i],b=polygon[j];if(((a[1]>point[1])!== (b[1]>point[1]))&&(point[0]<(b[0]-a[0])*(point[1]-a[1])/(b[1]-a[1])+a[0]))hit=!hit;}return hit;};
export class StudioViewport {
  constructor(canvas,{getState,onPaint,onSelectPart}) {
    this.canvas=canvas;this.ctx=canvas.getContext('2d');this.getState=getState;this.onPaint=onPaint;this.onSelectPart=onSelectPart;
    this.yaw=.72;this.pitch=.68;this.scale=52;this.panX=0;this.panY=0;this.hoverCell=null;this.pickFaces=[];
    this.dragging=false;this.dragMoved=false;this.down=null;this.animTime=0;this.animationFrame=0;this.lastFrame=0;this._fitNeeded=true;
    this.resizeObserver=new ResizeObserver(()=>this.resize());this.resizeObserver.observe(canvas.parentElement);
    canvas.addEventListener('pointerdown',e=>this.pointerDown(e));canvas.addEventListener('pointermove',e=>this.pointerMove(e));
    window.addEventListener('pointerup',e=>this.pointerUp(e));canvas.addEventListener('contextmenu',e=>e.preventDefault());
    canvas.addEventListener('wheel',e=>{e.preventDefault();this.scale=clamp(this.scale*Math.exp(-e.deltaY*.0013),3,300);this._fitNeeded=false;this.schedule();},{passive:false});
    this.resize();
  }
  resize(){const b=this.canvas.parentElement.getBoundingClientRect();this.width=Math.max(280,b.width);this.height=Math.max(200,b.height);const ratio=Math.min(window.devicePixelRatio||1,2);this.canvas.width=Math.round(this.width*ratio);this.canvas.height=Math.round(this.height*ratio);this.canvas.style.width=this.width+'px';this.canvas.style.height=this.height+'px';this.ratio=ratio;if(this._fitNeeded)this.fit();this.schedule();}
  fit(){const {project}=this.getState();if(!project)return;
    const dim=project.dimensions,pts=boxCorners([0,0,0],dim);
    const data=pts.map(pt=>this.projectPointRaw(pt));let minx=Infinity,miny=Infinity,maxx=-Infinity,maxy=-Infinity;
    for(const [x,y] of data){minx=Math.min(minx,x);miny=Math.min(miny,y);maxx=Math.max(maxx,x);maxy=Math.max(maxy,y);}
    this.scale=clamp(Math.min((this.width-130)/(maxx-minx+1e-3),(this.height-155)/(maxy-miny+1e-3)),5,115);
    this.panX=0;this.panY=15;this._fitNeeded=false;this.schedule();
  }
  resetFit(){this._fitNeeded=true;this.fit();}
  pointerDown(e){if(e.button>1)return;this.down={x:e.clientX,y:e.clientY,button:e.button,shift:e.shiftKey,alt:e.altKey,time:performance.now()};this.dragMoved=false;this.dragging=true;this.canvas.setPointerCapture?.(e.pointerId);}
  pointerMove(e){const r=this.canvas.getBoundingClientRect(), px=e.clientX-r.left,py=e.clientY-r.top;
    if(this.dragging && this.down) {
      const dx=e.clientX-this.down.x,dy=e.clientY-this.down.y;
      if(Math.abs(dx)>3||Math.abs(dy)>3)this.dragMoved=true;
      if(this.dragMoved){if(this.down.shift||this.down.button===1||this.down.alt){this.panX+=dx;this.panY+=dy;}else{this.yaw+=dx*.008;this.pitch=clamp(this.pitch+dy*.008,.18,1.38);}this.down.x=e.clientX;this.down.y=e.clientY;this._fitNeeded=false;this.schedule();}
    }else{const {mode,layer,project}=this.getState();this.hoverCell=mode==='build'?this.cellUnderMouse(px,py,layer,project):null;this.canvas.style.cursor=mode==='build'?'crosshair':'grab';this.schedule();}
  }
  pointerUp(e){if(!this.dragging)return;this.dragging=false;
    if(!this.dragMoved && this.canvas.contains(e.target)) {
      const r=this.canvas.getBoundingClientRect(),p=[e.clientX-r.left,e.clientY-r.top],s=this.getState();
      if(s.mode==='build'){
        const selected=this.hitTest(p,'cell'); const cell=selected?.cell?.[1]===s.layer ?selected.cell:this.cellUnderMouse(p[0],p[1],s.layer,s.project);
        if(cell)this.onPaint(...cell);
      }else {
        const face=this.hitTest(p,'part');this.onSelectPart(face?.partId||null);
      }
    }
    this.down=null;this.schedule();
  }
  hitTest(point,type){for(let i=this.pickFaces.length-1;i>=0;i--){const f=this.pickFaces[i];if(f.type===type&&inside(point,f.points))return f;}return null;}
  projectPointRaw([x,y,z]) {const c=Math.cos(this.yaw),s=Math.sin(this.yaw),sp=Math.sin(this.pitch),cp=Math.cos(this.pitch);return [x*c-z*s,(x*s+z*c)*sp-y*cp,(x*s+z*c)*cp+y*sp];}
  projectPoint(pt){const {project}=this.getState(),[w,h,d]=project.dimensions;const [x,y,z]=this.projectPointRaw([pt[0]-w/2,pt[1]-h/2,pt[2]-d/2]);return [this.width/2+this.panX+x*this.scale,this.height/2+this.panY+y*this.scale,z];}
  cellUnderMouse(px,py,layer,project){if(!project)return null;
    const s=(px-this.width/2-this.panX)/this.scale, t=(py-this.height/2-this.panY)/this.scale;
    const [w,h,d]=project.dimensions,sy=Math.sin(this.yaw),cy=Math.cos(this.yaw),sp=Math.sin(this.pitch),cp=Math.cos(this.pitch);
    const depth=(layer-h/2+cp*t)/sp;
    const rx=s, rz=t*sp+depth*cp;
    const x=w/2+rx*cy+rz*sy, z=d/2-rx*sy+rz*cy;
    const cell=[Math.floor(x+1e-6),layer,Math.floor(z+1e-6)];
    return cell.every((v,i)=>v>=0&&v<project.dimensions[i])?cell:null;
  }
  schedule(){if(!this.animationFrame)this.animationFrame=requestAnimationFrame(t=>this.draw(t));}
  draw(time){this.animationFrame=0;const s=this.getState();if(!s.project)return;
    const {project,mode,layer,selectedPart,selectedCell,playing,showAll,ghostBuild}=s;
    if(playing){if(this.lastFrame)this.animTime+=(time-this.lastFrame)/1000;this.lastFrame=time;}else this.lastFrame=0;
    const ctx=this.ctx;ctx.setTransform(this.ratio,0,0,this.ratio,0,0);ctx.clearRect(0,0,this.width,this.height);
    // Screen-space dark background with depth falloff.
    const bg=ctx.createRadialGradient(this.width*.48,this.height*.45,20,this.width*.48,this.height*.48,Math.max(this.width,this.height)*.76);
    bg.addColorStop(0,'#18232f');bg.addColorStop(1,'#0e151d');ctx.fillStyle=bg;ctx.fillRect(0,0,this.width,this.height);
    this.drawGrid(ctx,mode==='build'?layer:0,project);
    const camera=[Math.sin(this.yaw)*Math.cos(this.pitch),Math.sin(this.pitch),Math.cos(this.yaw)*Math.cos(this.pitch)];
    const faceObjects=[];
    if(mode==='build'||ghostBuild){
      for(const [key,role] of Object.entries(project.cells)){
        if(role==='Air')continue;
        const [x,y,z]=coordsOf(key);
        if(mode==='build'&&!showAll&&y>layer)continue;
        const mat=materialById(materialForBlock(project.palette[role])),corners=boxCorners([x,y,z],[x+1,y+1,z+1]);
        const alpha=mode!=='build'?.11 : (!showAll&&y<layer ? .58 : y>layer ? .20 : 1);
        for(const face of FACES){
          if(face.normal[0]*camera[0]+face.normal[1]*camera[1]+face.normal[2]*camera[2]<=.0001)continue;
          const [dx,dy,dz]=face.neighbor;
          if(project.cells[cellKey(x+dx,y+dy,z+dz)] && project.cells[cellKey(x+dx,y+dy,z+dz)]!=='Air')continue;
          faceObjects.push({points:face.points.map(idx=>this.projectPoint(corners[idx])),material:mat,face,alpha,type:'cell',cell:[x,y,z],selected:selectedCell===key});
        }
      }
    }
    if(mode!=='build') {
      for(const part of project.parts){if(part.visible===false)continue;
        const mat=materialById(part.material),pts=this.animatedBox(part,project,s.playing),selected=part.id===selectedPart;
        for(const face of FACES){
          const a=pts[face.points[0]],b=pts[face.points[1]],c=pts[face.points[2]];
          const ab=b.map((v,i)=>v-a[i]),ac=c.map((v,i)=>v-a[i]);
          const normal=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]];
          // Winding in FACES points toward the viewer for the outward side.
          if(normal[0]*camera[0]+normal[1]*camera[1]+normal[2]*camera[2]>=-.0001)continue;
          faceObjects.push({points:face.points.map(i=>this.projectPoint(pts[i])),material:mat,face,alpha:selected?1:.94,type:'part',partId:part.id,selected});
        }
      }
    }
    faceObjects.sort((a,b)=>a.points.reduce((n,p)=>n+p[2],0)/4 - b.points.reduce((n,p)=>n+p[2],0)/4);
    this.pickFaces=[];
    for(const obj of faceObjects){const pts=obj.points;ctx.beginPath();ctx.moveTo(pts[0][0],pts[0][1]);for(let i=1;i<pts.length;i++)ctx.lineTo(pts[i][0],pts[i][1]);ctx.closePath();
      ctx.fillStyle=shadeColor(obj.material.color,obj.face.shade,obj.alpha);ctx.fill();
      ctx.strokeStyle=obj.selected?'rgba(251,186,92,.85)':`rgba(8,15,20,${obj.alpha*.46})`;
      ctx.lineWidth=obj.selected?1.55:.75;ctx.stroke();
      if(obj.type==='part'&&obj.material.id==='wood'&&this.scale>18){this.drawWoodgrain(ctx,pts,obj.alpha);}
      if(obj.type==='cell'&&obj.cell[1]===layer&&mode==='build'){ctx.strokeStyle='rgba(255,255,255,.045)';ctx.stroke();}
      this.pickFaces.push({points:pts,type:obj.type,cell:obj.cell,partId:obj.partId});
    }
    if(mode==='build'&&this.hoverCell){const [x,y,z]=this.hoverCell;this.drawWireBox(ctx,[x,y,z],[x+1,y+1,z+1],'rgba(240,171,94,.96)',1.5);}
    if(mode!=='build'&&selectedPart){const part=project.parts.find(p=>p.id===selectedPart);if(part){this.drawWireBox(ctx,part.from,part.to,'rgba(244,177,91,.95)',1.5);}}
    this.drawCornerAxis(ctx);
    if(playing){this.schedule();}
  }
  animatedBox(part,project,playing){let corners=boxCorners(part.from,part.to);if(!part.motionId)return corners;
    const m=project.motions.find(m=>m.id===part.motionId);if(!m)return corners;
    let v=0;const t=this.animTime,phase=(m.phase||0)*Math.PI/180;
    if(m.type==='spin')v=t*(m.rpm||12)*Math.PI*2/60+phase;
    else v=Math.sin(t*(m.rpm||12)*Math.PI*2/60+phase)*(m.amplitude||.15);
    const pivot=m.pivot||part.from.map((x,i)=>(x+part.to[i])/2);
    return corners.map(p=>{
      const out=[...p];if(m.type==='slide'){out['xyz'.indexOf(m.axis)] += v;return out;}
      const axis='xyz'.indexOf(m.axis||'y'), a=(axis+1)%3,b=(axis+2)%3,c=Math.cos(v),s=Math.sin(v),ax=p[a]-pivot[a],bx=p[b]-pivot[b];out[a]=pivot[a]+ax*c-bx*s;out[b]=pivot[b]+ax*s+bx*c;return out;
    });
  }
  drawWoodgrain(ctx,pts,alpha){ // restrained timber stripe preview; not a texture or UV substitute.
    ctx.save();ctx.beginPath();ctx.moveTo(...pts[0].slice(0,2));for(let i=1;i<pts.length;i++)ctx.lineTo(...pts[i].slice(0,2));ctx.closePath();ctx.clip();
    ctx.strokeStyle=`rgba(37,19,9,${.12*alpha})`;ctx.lineWidth=1;
    for(let t=.22;t<1;t+=.28){const x=pts[0][0]*(1-t)+pts[3][0]*t,y=pts[0][1]*(1-t)+pts[3][1]*t,X=pts[1][0]*(1-t)+pts[2][0]*t,Y=pts[1][1]*(1-t)+pts[2][1]*t;ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(X,Y);ctx.stroke();}
    ctx.restore();
  }
  drawGrid(ctx,y,project){const [w,,d]=project.dimensions;ctx.lineWidth=.8;
    for(let x=0;x<=w;x++){const a=this.projectPoint([x,y,0]),b=this.projectPoint([x,y,d]);ctx.strokeStyle=x===0||x===w?'rgba(156,182,198,.3)':'rgba(156,182,198,.11)';ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(b[0],b[1]);ctx.stroke();}
    for(let z=0;z<=d;z++){const a=this.projectPoint([0,y,z]),b=this.projectPoint([w,y,z]);ctx.strokeStyle=z===0||z===d?'rgba(156,182,198,.3)':'rgba(156,182,198,.11)';ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(b[0],b[1]);ctx.stroke();}
    const a=this.projectPoint([0,0,0]),x=this.projectPoint([1,0,0]),yp=this.projectPoint([0,1,0]),z=this.projectPoint([0,0,1]);
    [[x,'#ca7362'],[yp,'#86b397'],[z,'#6a9ec5']].forEach(([pt,color])=>{ctx.strokeStyle=color;ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(pt[0],pt[1]);ctx.stroke();});
  }
  drawWireBox(ctx,from,to,color,lineWidth){const pts=boxCorners(from,to).map(p=>this.projectPoint(p));const edges=[[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]];
    ctx.strokeStyle=color;ctx.lineWidth=lineWidth;for(const [a,b] of edges){ctx.beginPath();ctx.moveTo(...pts[a].slice(0,2));ctx.lineTo(...pts[b].slice(0,2));ctx.stroke();}
  }
  drawCornerAxis(ctx){const ox=this.width-57,oy=this.height-53;ctx.font='600 10px system-ui';ctx.textAlign='center';const ax=[[1,0,0,'X','#c87764'],[0,1,0,'Y','#75bd91'],[0,0,1,'Z','#629fd3']];
    for(const [x,y,z,label,color] of ax){const [sx,sy]=this.projectPointRaw([x,y,z]);const px=ox+sx*25,py=oy+sy*25;ctx.strokeStyle=color;ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(ox,oy);ctx.lineTo(px,py);ctx.stroke();ctx.fillStyle=color;ctx.fillText(label,px+(sx>=0?6:-6),py+(sy>=0?10:-5));}
    ctx.fillStyle='#c0c9d2';ctx.beginPath();ctx.arc(ox,oy,3,0,Math.PI*2);ctx.fill();
  }
}
