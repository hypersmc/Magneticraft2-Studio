// Lightweight DOM/canvas smoke test for module initialization, 3D rendering and mode switching.
// Not a visual browser test: container Chromium is centrally blocked from local websites.
import assert from 'node:assert/strict';
class Elem {
 constructor(id='') {this.id=id;this.style={};this.dataset={};this.events={};this.value='';this.innerHTML='';this.textContent='';this.checked=false;this.files=[];this.classList={toggle(){},add(){},remove(){}};this.parentElement={getBoundingClientRect:()=>({width:770,height:650})};}
 addEventListener(key, fn) {this.events[key]=fn;}
 setAttribute(){}
 appendChild(){}
 remove(){}
 contains(){return true;}
 getBoundingClientRect(){return {left:0,top:0,width:770,height:650};}
 getContext(){return ctx;}
 getScreenCTM(){return {inverse(){return {}}};}
 querySelector(){return new Elem('rect');}
 createSVGPoint(){return {x:0,y:0,matrixTransform(){return {x:0,y:0}}};}
}
const ctx=new Proxy({createRadialGradient(){return {addColorStop(){}}}}, {get(target,key){return key in target ?target[key]:()=>{};},set(target,key,value){target[key]=value;return true;}});
const elements=new Map();const el=id=>{if(!elements.has(id))elements.set(id,new Elem(id));return elements.get(id)};
const tabs=['build','model','motion'].map(mode=>{const e=new Elem('tab');e.dataset.mode=mode;return e;});
globalThis.document={getElementById:el,querySelectorAll:()=>tabs,createElement:()=>new Elem('created'),body:new Elem('body')};
globalThis.window={devicePixelRatio:1,addEventListener(){},confirm:()=>true,prompt:()=>null};
globalThis.localStorage={getItem(){return null;},setItem(){}};
globalThis.ResizeObserver=class{observe(){}};
const queue=[];globalThis.requestAnimationFrame=fn=>{queue.push(fn);return queue.length;};
await import('../src/app.mjs');
assert.equal(window.__studio_debug.state.mode,'build');
assert.match(el('left-sidebar').innerHTML,/MechanicalInput|Block palette/);
assert.match(el('right-sidebar').innerHTML,/Structure dimensions|STRUCTURE DIMENSIONS/);
const callback=queue.shift();if(callback)callback(1000);
assert.ok(window.__studio_debug.viewport.pickFaces.length>0,'3D construction geometry should render');
tabs[1].events.click();
assert.equal(window.__studio_debug.state.mode,'model');
const cb2=queue.shift();if(cb2)cb2(1020);
assert.ok(window.__studio_debug.viewport.pickFaces.length>0,'Formed geometry should render');
window.__studio_debug.addPart({name:'Test new cuboid'});
assert.equal(window.__studio_debug.state.project.parts.at(-1).name,'Test new cuboid');
const action=(side,verb)=>el(side).events.click({target:{closest(){return {dataset:{action:verb}};}}});
action('left-sidebar','add-motion');
assert.equal(window.__studio_debug.state.mode,'motion');
assert.equal(window.__studio_debug.state.project.motions.length,1);
const motion=window.__studio_debug.state.project.motions[0];
const right=el('right-sidebar');
right.events.change({target:{dataset:{partVec:'pivot',axis:'2'},value:'1.25'}});
assert.equal(motion.pivot[2],1.25);
action('dock-actions','toggle-play');
window.__studio_debug.viewport.draw(1200);
window.__studio_debug.viewport.draw(1400);
assert.ok(window.__studio_debug.viewport.pickFaces.length>0);
console.log('UI boot, construction/form preview, part insertion, animation and pivot editing passed.');
process.exit(0);
