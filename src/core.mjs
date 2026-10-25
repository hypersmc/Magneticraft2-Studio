// Magneticraft2 Studio v0.1 — document + Forge 1.20.1 export compiler.
// Coordinate system: X east, Y up, Z south. All model positions are in BLOCKS,
// with 1/16-block snapping in the editor; JSON block models use pixels.
export const SCHEMA_VERSION = 1;
export const ORIENTATIONS = ['north', 'east', 'south', 'west'];
export const DEFAULT_PALETTE = {
  Air: 'minecraft:air',
  Frame: 'minecraft:oak_planks',
  Controller: 'magneticraft2:mechanical_ore_washer',
  ItemInput: 'magneticraft2:multiblock_item_input',
  ItemOutput: 'magneticraft2:multiblock_item_output',
  MechanicalInput: 'magneticraft2:mechanical_input_module',
  FluidInput: 'magneticraft2:multiblock_fluid_input',
  FluidOutput: 'magneticraft2:multiblock_fluid_output',
};
export const MATERIALS = [
  { id: 'wood', name: 'Oak wood', color: '#a87d4f', texture: 'minecraft:block/oak_planks' },
  { id: 'copper', name: 'Copper', color: '#b66d4c', texture: 'minecraft:block/copper_block' },
  { id: 'stone', name: 'Stone', color: '#87909b', texture: 'minecraft:block/stone' },
  { id: 'iron', name: 'Iron', color: '#b7c0c4', texture: 'minecraft:block/iron_block' },
  { id: 'dark', name: 'Dark metal', color: '#59666f', texture: 'minecraft:block/deepslate_tiles' },
  { id: 'glass', name: 'Glass', color: '#6aada8', texture: 'minecraft:block/glass' },
  { id: 'water', name: 'Water', color: '#458cbd', texture: 'minecraft:block/water_still' },
];
let sequence = 1;
export const uid = () => 'part_' + (sequence++).toString(36) + '_' + Math.random().toString(36).slice(2, 6);
export const clamp = (n, lo, hi) => Math.min(hi, Math.max(lo, n));
export const cleanSlug = s => String(s || 'machine').toLowerCase().replace(/[^a-z0-9_]+/g, '_').replace(/^_+|_+$/g, '') || 'machine';
export const cellKey = (x, y, z) => `${x},${y},${z}`;
export const coordsOf = key => key.split(',').map(Number);
export const clone = obj => JSON.parse(JSON.stringify(obj));
export const snap = x => Math.round(Number(x) * 16) / 16;
export const materialById = id => MATERIALS.find(m => m.id === id) || MATERIALS[0];
export const materialForBlock = block => {
  const b = String(block || '');
  return /copper|terracotta|brick/.test(b) ? 'copper' : /iron|steel/.test(b) ? 'iron' : /water/.test(b) ? 'water' : /glass/.test(b) ? 'glass' : /stone|deepslate|cobble/.test(b) ? 'stone' : 'wood';
};
export function makeProject(name = 'New Multiblock', dimensions = [5, 3, 5]) {
  return {
    studioVersion: SCHEMA_VERSION,
    name, id: cleanSlug(name), dimensions: [...dimensions],
    palette: clone(DEFAULT_PALETTE), cells: {}, parts: [], motions: [],
    settings: { inventory: true, power: false, fuel: false, fluid: false, inputs: [], outputs: [] },
    notes: '',
  };
}
export function makeDemo() {
  const p = makeProject('Workshop Ore Washer', [5, 3, 5]);
  p.id = 'workshop_ore_washer';
  for (let x = 0; x < 5; x++) for (let z = 0; z < 5; z++) {
    if (x === 0 || x === 4 || z === 0 || z === 4) p.cells[cellKey(x, 0, z)] = 'Frame';
  }
  for (const x of [0, 4]) for (const z of [0, 4]) {
    p.cells[cellKey(x, 1, z)] = 'Frame'; p.cells[cellKey(x, 2, z)] = 'Frame';
  }
  p.cells[cellKey(2, 0, 2)] = 'Controller';
  p.cells[cellKey(2, 0, 0)] = 'ItemInput';
  p.cells[cellKey(2, 0, 4)] = 'ItemOutput';
  p.cells[cellKey(0, 0, 2)] = 'MechanicalInput';
  p.cells[cellKey(4, 0, 2)] = 'FluidInput';
  const add = (name, from, to, material, extra = {}) => p.parts.push({id: uid(), name, group:'Frame', from, to, material, visible:true, ...extra});
  for (const x of [.2, 4.48]) for (const z of [.2, 4.48]) add('Timber post', [x, 0, z], [x+.32, 2.85, z+.32], 'wood');
  for (const y of [.28, 2.60]) for (const z of [.2, 4.48]) add('Cross beam', [.2,y,z], [4.8,y+.24,z+.32], 'wood');
  add('Lower trough', [.65,.25,.75], [4.35,.45,4.25], 'copper');
  add('Water surface', [.75,.46,.85], [4.25,.48,4.15], 'water', {group:'Water'});
  add('Drum', [1.25,1.05,1.15], [3.75,2.15,3.85], 'copper', {group:'Rotor'});
  add('Main shaft', [.4,1.57,2.38], [4.6,1.72,2.62], 'iron', {group:'Rotor'});
  add('Output chute', [1.7,.46,4.15], [3.3,.67,5.0], 'wood');
  return p;
}
export function normalizeProject(p) {
  if (!p || !Array.isArray(p.dimensions) || p.dimensions.length !== 3) throw new Error('Invalid Studio project: dimensions missing');
  p.dimensions = p.dimensions.map(n => clamp(Math.floor(Number(n) || 1), 1, 128));
  p.name = String(p.name || 'New Multiblock'); p.id = cleanSlug(p.id || p.name);
  p.palette = { Air:'minecraft:air', ...(p.palette || {}) }; p.cells ||= {};
  p.parts = (p.parts || []).map(part => ({ id: uid(), group:'Static', visible:true, material:'wood', ...part }));
  p.motions ||= []; p.settings ||= {};
  return p;
}
export function importLegacyStructure(json) {
  const s = json?.structure;
  if (!s || !Array.isArray(s.dimensions) || !s.layout || !s.blocks) throw new Error('Not a Magneticraft multiblock structure JSON');
  const sourceDir = (json.name || '').match(/_(north|east|south|west)$/)?.[1] || 'north';
  const inverse = {north:'north',east:'west',south:'south',west:'east'}[sourceDir];
  const id = cleanSlug((json.name || 'imported_machine').replace(/_(north|east|south|west)$/, ''));
  const p = makeProject(id.replaceAll('_', ' ').replace(/\b\w/g, ch => ch.toUpperCase()), rotatedDimensions(s.dimensions,inverse));
  p.id = id; p.palette = { Air:'minecraft:air', ...s.blocks };
  p.omittedAirInSource = !Object.hasOwn(s.blocks, 'Air');
  p.settings = { ...p.settings, ...(json.settings || {}) };
  delete p.settings.replacewhenformed;
  const [width, height, depth] = s.dimensions;
  for (let y=0; y<height; y++) {
    const rows = s.layout[`layer${y+1}`];
    if (!Array.isArray(rows) || rows.length !== depth) throw new Error(`Malformed layer ${y+1}`);
    for (let z=0; z<depth; z++) {
      if (!Array.isArray(rows[z]) || rows[z].length !== width) throw new Error(`Malformed row at Y=${y}, Z=${z}`);
      for (let x=0; x<width; x++) if (rows[z][x] !== 'Air') {
        const [rx,rz]=rotateCell(x,z,s.dimensions,inverse);
        p.cells[cellKey(rx,y,rz)] = rows[z][x];
      }
    }
  }
  return p;
}
export function rotatedDimensions(dim, dir) { return ['east','west'].includes(dir) ? [dim[2],dim[1],dim[0]] : [...dim]; }
export function rotateCell(x, z, dims, dir) {
  const [w,,d] = dims;
  switch (dir) {
    case 'east': return [d-1-z, x];
    case 'south': return [w-1-x, d-1-z];
    case 'west': return [z, w-1-x];
    default: return [x,z];
  }
}
export function rotatePoint(x, z, dims, dir) {
  const [w,,d] = dims;
  switch (dir) {
    case 'east': return [d-z, x];
    case 'south': return [w-x, d-z];
    case 'west': return [z, w-x];
    default: return [x,z];
  }
}
export function rotateBox(from, to, dim, dir) {
  const a=rotatePoint(from[0],from[2],dim,dir), b=rotatePoint(to[0],to[2],dim,dir);
  return { from:[Math.min(a[0],b[0]),from[1],Math.min(a[1],b[1])], to:[Math.max(a[0],b[0]),to[1],Math.max(a[1],b[1])] };
}
export function makeLegacyStructure(p, dir='north') {
  const dims = rotatedDimensions(p.dimensions,dir), [w,h,d]=dims;
  if(w*h*d>150000) throw new Error('Machine structure too large to export as a dense layer JSON (150,000 cell safety limit)');
  const layers = Array.from({length:h}, () => Array.from({length:d}, () => Array(w).fill('Air')));
  for (const [key, role] of Object.entries(p.cells)) {
    const [x,y,z] = coordsOf(key), [rx,rz] = rotateCell(x,z,p.dimensions,dir);
    if (y>=0 && y<h && rx>=0 && rx<w && rz>=0 && rz<d) layers[y][rz][rx] = role;
  }
  const layout={}; layers.forEach((l,i)=> layout[`layer${i+1}`]=l);
  const blocks={...p.palette};
  if(p.omittedAirInSource && !layers.some(layer=>layer.some(row=>row.includes('Air')))) delete blocks.Air;
  return {
    name: `${p.id}_${dir}`,
    structure: {dimensions:dims,layout,blocks},
    settings: {...p.settings,replacewhenformed:`multiblock/${p.id}_${dir}`},
  };
}
const nearly = (a,b) => Math.abs(a-b)<1e-7;
function modelFaceUV(face, from, to) {
  const [x0,y0,z0]=from,[x1,y1,z1]=to;
  if (face==='north'||face==='south') return [x0*16, (1-y1)*16, x1*16, (1-y0)*16].map(n=>+n.toFixed(4));
  if (face==='east'||face==='west') return [z0*16, (1-y1)*16, z1*16, (1-y0)*16].map(n=>+n.toFixed(4));
  return [x0*16,z0*16,x1*16,z1*16].map(n=>+n.toFixed(4));
}
export function sliceStaticParts(p, dir='north') {
  const dims=rotatedDimensions(p.dimensions,dir), [w,h,d]=dims;
  const cells = new Map(); const warnings=[];
  for (const part of p.parts) {
    if (part.visible===false) continue;
    if (part.motionId) continue; // animated geometry cannot be baked statically
    const srcFrom=part.from.map(Number), srcTo=part.to.map(Number);
    if(srcFrom.some((v,i)=>!(v<srcTo[i]))) {warnings.push(`Skipped invalid box: ${part.name}`);continue;}
    const rb=rotateBox(srcFrom,srcTo,p.dimensions,dir);
    if(rb.from.some((v,i)=>v<0) || rb.to.some((v,i)=>v>dims[i])) warnings.push(`Box outside construction bounds: ${part.name}`);
    const xlo=Math.max(0,Math.floor(rb.from[0])), xhi=Math.min(w,Math.ceil(rb.to[0]));
    const ylo=Math.max(0,Math.floor(rb.from[1])), yhi=Math.min(h,Math.ceil(rb.to[1]));
    const zlo=Math.max(0,Math.floor(rb.from[2])), zhi=Math.min(d,Math.ceil(rb.to[2]));
    for(let y=ylo; y<yhi; y++) for(let z=zlo; z<zhi; z++) for(let x=xlo; x<xhi; x++) {
      const lo=[Math.max(rb.from[0],x), Math.max(rb.from[1],y), Math.max(rb.from[2],z)];
      const hi=[Math.min(rb.to[0],x+1),Math.min(rb.to[1],y+1),Math.min(rb.to[2],z+1)];
      if(lo.some((v,i)=>v>=hi[i]-1e-7))continue;
      const key=cellKey(x,y,z);
      if(!cells.has(key)) cells.set(key,[]);
      const tex=part.texture || materialById(part.material).texture;
      const faceDefs={};
      const faces = [
        ['down',1,rb.from[1],lo[1]],['up',1,rb.to[1],hi[1]],
        ['north',2,rb.from[2],lo[2]],['south',2,rb.to[2],hi[2]],
        ['west',0,rb.from[0],lo[0]],['east',0,rb.to[0],hi[0]]
      ];
      const localFrom=lo.map((a,i)=>a-[x,y,z][i]),localTo=hi.map((a,i)=>a-[x,y,z][i]);
      for(const [side,_axis,bound,piece] of faces) if(nearly(bound,piece)) faceDefs[side]={texture:'#all',uv:modelFaceUV(side,localFrom,localTo)};
      const element={from:localFrom.map(a=>+((a*16).toFixed(4))),to:localTo.map(a=>+((a*16).toFixed(4))),faces:faceDefs};
      cells.get(key).push({element,tex});
    }
  }
  const output={};
  for(const [key,pieces] of cells) {
    const textures={}; const elements=[]; const textureKeys=new Map();
    for(const piece of pieces) {
      let t=textureKeys.get(piece.tex);
      if(!t){t=`t${textureKeys.size}`;textureKeys.set(piece.tex,t);textures[t]=piece.tex;}
      const el=clone(piece.element);
      for(const face of Object.values(el.faces)) face.texture=`#${t}`;
      elements.push(el);
    }
    output[key]={parent:'minecraft:block/block',textures,elements};
  }
  return {models:output,warnings};
}
export function compileExportFiles(p) {
  const files={}; const slug=p.id; const manifest={format:1,id:slug,dimensions:p.dimensions,orientation:'north',models:{},motions:p.motions,animatedParts:p.parts.filter(part=>part.motionId),warnings:[]};
  files[`studio/${slug}.mcraftstudio.json`]=JSON.stringify(p,null,2);
  const controllerCount=Object.values(p.cells).filter(role=>role==='Controller').length;
  if(controllerCount!==1) manifest.warnings.push(`Expected exactly one Controller in the construction, found ${controllerCount}`);
  for(const dir of ORIENTATIONS) {
    const legacy=makeLegacyStructure(p,dir);
    files[`src/main/resources/data/magneticraft2/multiblocks/${slug}_${dir}.json`]=JSON.stringify(legacy,null,2);
    const split=sliceStaticParts(p,dir), modelPaths={};
    for(const [key,model] of Object.entries(split.models)) {
      const [x,y,z]=coordsOf(key), modelLoc=`block/studio/${slug}/${dir}/${x}_${y}_${z}`;
      files[`src/main/resources/assets/magneticraft2/models/${modelLoc}.json`]=JSON.stringify(model,null,2);
      modelPaths[key]=`magneticraft2:${modelLoc}`;
    }
    const controllerEntry=Object.entries(p.cells).find(([key,v])=>v==='Controller');
    let controllerModel={parent:'minecraft:block/block',textures:{particle:'minecraft:block/oak_planks'},elements:[]};
    if(controllerEntry){
      const [cx,cy,cz]=coordsOf(controllerEntry[0]),[rx,rz]=rotateCell(cx,cz,p.dimensions,dir);
      controllerModel=split.models[cellKey(rx,cy,rz)] || controllerModel;
    }
    files[`src/main/resources/assets/magneticraft2/models/multiblock/${slug}_${dir}.json`]=JSON.stringify(controllerModel,null,2);
    manifest.models[dir]={dimensions:rotatedDimensions(p.dimensions,dir),cells:modelPaths};
    manifest.warnings.push(...split.warnings.map(msg=>`${dir}: ${msg}`));
  }
  files[`src/main/resources/assets/magneticraft2/studio/${slug}_manifest.json`]=JSON.stringify(manifest,null,2);
  files['EXPORT_README.txt']=`Magneticraft2 Studio 0.1 alpha - ${p.name}\n\nSTRUCTURE: The four JSONs under data/magneticraft2/multiblocks/ follow the existing project format.\nSTATIC VISUALS: Each model under assets/magneticraft2/models/block/studio/ is a valid Minecraft block-model slice (coordinates within 0..16).\nCONTROLLER FALLBACK: models/multiblock/<id>_<direction>.json includes only the controller cell's static slice.\n\nIMPORTANT - GAME INTEGRATION IS NOT FINISHED:\nCurrent Magneticraft2 renders its formed static body at the controller. Displaying every other model slice across formed filler blocks REQUIRES new code in the mod reading the manifest and assigning cell models. Animated parts and motions are EDITOR PREVIEW METADATA ONLY until a runtime animator is implemented.\nCollision, lighting, chunk boundary, atlas registration, culling and JEI/book integration still require validation in game.\nThe editor does not generate a fully working multiblock renderer yet. Do not install these assets into a production build expecting complete visuals.\n\nThe .mcraftstudio.json file is editable and can be reopened in Studio.\n`;
  return files;
}
