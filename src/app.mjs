import {makeProject,makeDemo,normalizeProject,importLegacyStructure,compileExportFiles,cleanSlug,cellKey,coordsOf,clamp,uid,snap,MATERIALS,materialById,materialForBlock,clone} from './core.mjs';
import {createZip} from './zip.mjs';
import {StudioViewport} from './viewport.mjs';
const $=id=>document.getElementById(id);
const escapeHTML=str=>String(str??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt=n=>Number(n.toFixed(4)).toString();
const state={project:makeDemo(),mode:'build',layer:0,selectedRole:'Frame',selectedCell:null,selectedPart:null,playing:false,showAll:false,ghostBuild:false,dirty:false};
const autosaveKey='magneticraft2-studio-local-v1';
try{const saved=localStorage.getItem(autosaveKey);if(saved) state.project=normalizeProject(JSON.parse(saved));}catch(err){console.warn('Ignored invalid autosave',err);}
function report(msg,error=false){$('status-message').textContent=msg.toUpperCase();const parent=$('toast-container'),el=document.createElement('div');el.className='toast'+(error?' error':'');el.textContent=msg;parent.appendChild(el);setTimeout(()=>el.remove(),3200);}
let persistenceTimer=0;
function changed(msg='UNSAVED EDITS'){state.dirty=true;clearTimeout(persistenceTimer);persistenceTimer=setTimeout(()=>{try{localStorage.setItem(autosaveKey,JSON.stringify(state.project));}catch(e){console.warn(e);}},240);refresh();$('status-message').textContent=msg;}
function refresh(){renderHeader();renderSidebar();renderInspector();renderDock();viewport.schedule();}
function renderHeader(){const p=state.project;$('project-name').textContent=p.name;$('project-dim').textContent=p.dimensions.join(' × ');document.title=`${p.name} — Magneticraft2 Studio`;
  document.querySelectorAll('.mode-tab').forEach(el=>{el.classList.toggle('active',el.dataset.mode===state.mode);el.setAttribute('aria-selected',String(el.dataset.mode===state.mode));});
  $('viewport-mode').textContent=state.mode==='build'?'CONSTRUCTION VIEW':state.mode==='model'?'FORMED MODEL VIEW':'MOTION PREVIEW';
  $('viewport-help').textContent=state.mode==='build'?'DRAG TO ORBIT · SHIFT+DRAG TO PAN · SCROLL TO ZOOM · CLICK TO PAINT':'DRAG TO ORBIT · CLICK TO SELECT · SHIFT+DRAG TO PAN · SCROLL TO ZOOM';
  $('show-all-wrap').classList.toggle('hide',state.mode!=='build');$('ghost-wrap').classList.toggle('hide',state.mode==='build');
  $('toggle-all').checked=state.showAll;$('toggle-ghost').checked=state.ghostBuild;
  $('bottom-pane').setAttribute('aria-label',state.mode==='build'?'Construction layer editor':'Formed model top view');
  $('layer-label').textContent=state.mode==='build'?`Y = ${state.layer}`:state.mode==='model'?'PLAN / X–Z':`PREVIEW / ${state.playing?'PLAYING':'PAUSED'}`;
  $('status-data').textContent=`${Object.keys(p.cells).length} CELLS · ${p.parts.length} MODEL PARTS · ${p.motions.length} MOTIONS`;
}
function section(head,body,{count='',action=''}={}){return `<div class="section-header"><span>${head}</span><span>${action||`<span class="count">${count}</span>`}</span></div><div class="section-body">${body}</div>`;}
function renderSidebar(){const p=state.project,roleCount={};Object.values(p.cells).forEach(role=>roleCount[role]=(roleCount[role]||0)+1);
  let html=section('Workspace',`<button class="explorer-item ${state.mode==='build'?'active':''}" data-action="mode" data-value="build"><span class="tree-icon">▦</span><span class="item-name">Construction layout</span></button><button class="explorer-item ${state.mode==='model'?'active':''}" data-action="mode" data-value="model"><span class="tree-icon">⬡</span><span class="item-name">Formed geometry</span></button><button class="explorer-item ${state.mode==='motion'?'active':''}" data-action="mode" data-value="motion"><span class="tree-icon">↻</span><span class="item-name">Animation rig</span></button>`);
  if(state.mode==='build'){
    html+=`<div class="border-bottom"></div>`+section('Block palette',Object.entries(p.palette).map(([role,block])=>{
      const color=materialById(materialForBlock(block)).color;
      return `<button class="palette-item ${state.selectedRole===role?'active':''}" data-action="role" data-value="${escapeHTML(role)}" title="${escapeHTML(block)}"><span class="swatch" style="background:${escapeHTML(role==='Air'?'#192934':color)}"></span><span class="item-name">${escapeHTML(role)}</span><span class="item-detail">${roleCount[role]||'·'}</span></button>`;
    }).join(''),{action:'<button class="little-action" data-action="add-role">＋ ROLE</button>'});
    html+=`<div class="border-bottom"></div>`+section('Layers',`<div class="layer-select"><input id="layer-slider" type="range" min="0" max="${p.dimensions[1]-1}" value="${state.layer}"/><span>Y ${state.layer}/${p.dimensions[1]-1}</span></div><div class="model-tools" style="margin-top:13px"><button class="little-action" data-action="layer-prev">− Down</button><button class="little-action" data-action="layer-next">＋ Up</button></div>`);
    html+=`<div class="border-bottom"></div>`+section('Existing machines',`<button class="explorer-item" data-action="sample" data-value="mechanical_ore_washer"><span class="tree-icon">↥</span><span class="item-name">Import Ore Washer</span></button><button class="explorer-item" data-action="sample" data-value="mechanical_sifter"><span class="tree-icon">↥</span><span class="item-name">Import Sifter</span></button>`);
    html+=`<div class="left-footer-note">Select a block role, then click cells in the 2D layer editor or the 3D construction preview. Use <strong>Air</strong> to erase a block.</div>`;
  }else{
    const groups=new Map();p.parts.forEach(part=>{const key=part.group||'Static';if(!groups.has(key))groups.set(key,[]);groups.get(key).push(part);});
    let body='';for(const [group,parts] of groups){body+=`<div class="section-header" style="padding:10px 2px 5px">${escapeHTML(group)} <span class="count">${parts.length}</span></div>`;
      body+=parts.map(part=>`<button class="part-item ${state.selectedPart===part.id?'active':''}" data-action="select-part" data-value="${escapeHTML(part.id)}"><span class="swatch" style="background:${materialById(part.material).color}"></span><span class="item-name">${escapeHTML(part.name)}</span><span class="item-detail">${part.motionId?'↻':'⬡'}</span></button>`).join('');
    }
    html+=`<div class="border-bottom"></div>`+section('Model hierarchy',`<div class="scene-items">${body||'<p class="empty-hint">No geometry. Add a cuboid to start building the formed appearance.</p>'}</div>`,{action:'<button class="little-action" data-action="add-part">＋ CUBE</button>'});
    html+=`<div class="border-bottom"></div>`+section('Quick tools',`<div class="model-tools"><button class="little-action" data-action="add-frame">Frame preset</button><button class="little-action" data-action="make-block-models">From structure</button><button class="little-action" data-action="duplicate-part" ${state.selectedPart?'':'disabled'}>Duplicate</button></div>`);
    if(state.mode==='motion')html+=`<div class="border-bottom"></div>`+section('Motions',`<div class="scene-items">${p.motions.map(m=>`<button class="motion-item ${p.parts.some(pt=>pt.id===state.selectedPart&&pt.motionId===m.id)?'active':''}" data-action="select-motion" data-value="${escapeHTML(m.id)}"><span class="tree-icon">↻</span><span class="item-name">${escapeHTML(m.name)}</span><span class="item-detail">${escapeHTML(m.type)}</span></button>`).join('')||'<p class="empty-hint">Add a motion on the right, then assign it to model parts.</p>'}</div>`,{action:'<button class="little-action" data-action="add-motion">＋ MOTION</button>'});
    html+=`<div class="left-footer-note">Create geometry in full Minecraft-block coordinates. Each component can span many blocks. Select a part to edit it or drag it in the plan view below.</div>`;
  }
  $('left-sidebar').innerHTML=html;
}
const coordFields=(prefix,vec)=>`<div class="xyz-markers">${['X','Y','Z'].map((letter,i)=>`<label>${letter}<input type="number" class="prop-input" data-part-vec="${prefix}" data-axis="${i}" step="0.0625" value="${fmt(Number(vec[i]))}" aria-label="${prefix} ${letter}" /></label>`).join('')}</div>`;
function renderInspector(){const p=state.project;
  let title='Inspector',subtitle='',body='';
  if(state.mode==='build'){
    const [x,y,z]=state.selectedCell?coordsOf(state.selectedCell):[null,null,null];
    title='Construction properties';subtitle='Block roles, structure and dimensions';
    body=`<div class="property-section"><div class="prop-head">STRUCTURE DIMENSIONS</div><div class="xyz-markers">${['WIDTH X','HEIGHT Y','DEPTH Z'].map((label,i)=>`<label>${label}<input class="prop-input" type="number" id="dim-${i}" min="1" max="128" value="${p.dimensions[i]}" /></label>`).join('')}</div><button class="outline-btn field-button" data-action="resize">Apply dimensions</button><p class="prop-help">Up to 128 blocks per axis in the editor. Large exports have a safety limit.</p></div>
    <div class="property-section"><div class="prop-head">PAINT BRUSH</div><label class="prop-label">Selected role</label><select id="brush-role" class="prop-select">${Object.keys(p.palette).map(role=>`<option value="${escapeHTML(role)}" ${role===state.selectedRole?'selected':''}>${escapeHTML(role)}</option>`).join('')}</select><label class="prop-label">Registry block ID</label><input id="palette-block" class="prop-input" value="${escapeHTML(p.palette[state.selectedRole]||'')}" /><p class="prop-help">Edit the registry ID for the selected role. The ID is used in the exported structure JSON.</p></div>
    <div class="property-section"><div class="prop-head">SELECTED CONSTRUCTION CELL</div>${state.selectedCell?`<div class="readout">X ${x} · Y ${y} · Z ${z}</div><p class="prop-help">Current block: <strong>${escapeHTML(p.cells[state.selectedCell]||'Air')}</strong></p><button class="outline-btn field-button" data-action="erase-cell">Erase selected cell</button>`:'<p class="prop-help">Click a cell to edit its construction block.</p>'}</div>
    <div class="property-section"><div class="prop-head">STRUCTURE SETTINGS</div>${['inventory','power','fuel','fluid'].map(k=>`<label class="property-switch"><input type="checkbox" data-setting="${k}" ${p.settings[k]?'checked':''}/> ${k.charAt(0).toUpperCase()+k.slice(1)}</label>`).join('')}</div>`;
  }else{
    const part=p.parts.find(part=>part.id===state.selectedPart);
    if(part){const m=materialById(part.material);
      title='Geometry properties';subtitle='Edit position, size, material and movement';
      body=`<div class="property-section"><div class="prop-head">SELECTED CUBOID</div><label class="prop-label">Name</label><input class="prop-input" data-part-text="name" value="${escapeHTML(part.name)}"/><label class="prop-label">Group / assembly</label><input class="prop-input" data-part-text="group" value="${escapeHTML(part.group||'Static')}"/><label class="property-switch"><input type="checkbox" data-part-toggle="visible" ${part.visible!==false?'checked':''}/> Visible in preview and export</label></div>
      <div class="property-section"><div class="prop-head">POSITION / FROM (BLOCKS)</div>${coordFields('from',part.from)}<p class="prop-help">Coordinates can be negative or exceed the construction dimensions. Export will warn about geometry outside its footprint.</p><div class="prop-head" style="margin-top:17px">END / TO (BLOCKS)</div>${coordFields('to',part.to)}<p class="prop-help">1 model pixel = 0.0625 blocks. All entries snap to 1/16.</p></div>
      <div class="property-section"><div class="prop-head">MATERIAL</div><label class="prop-label">Preview material</label><select id="part-material" class="prop-select">${MATERIALS.map(opt=>`<option value="${opt.id}" ${part.material===opt.id?'selected':''}>${escapeHTML(opt.name)}</option>`).join('')}</select><label class="prop-label">Minecraft texture resource</label><input id="part-texture" class="prop-input" value="${escapeHTML(part.texture||m.texture)}" /><p class="prop-help">The exporter writes this resource path into the Minecraft model JSON. Preview colors are representative, not final game textures.</p></div>
      ${state.mode==='motion'?renderMotionProperty(p,part):`<div class="property-section"><div class="prop-head">PART ACTIONS</div><button class="outline-btn field-button" data-action="duplicate-part">Duplicate part</button><button class="outline-btn danger field-button" data-action="remove-part">Delete selected part</button></div>`}`;
    }else{
      title=state.mode==='motion'?'Animation properties':'Geometry properties';subtitle='Nothing selected';body=`<div class="property-section"><div class="prop-head">${state.mode==='motion'?'MOTION RIG':'MODEL ELEMENTS'}</div><p class="prop-help">${state.mode==='motion'?'Select a cube, add a motion, and assign it to the cube.':'Create a cube to begin building your custom formed machine.'}</p><button class="primary-btn field-button" data-action="${state.mode==='motion'?'add-motion':'add-part'}">${state.mode==='motion'?'＋ Add motion':'＋ Add cuboid'}</button></div>`;
    }
  }
  $('right-sidebar').innerHTML=`<div class="panel-title">${title}<div class="panel-subtitle">${subtitle}</div></div><div class="prop-content">${body}</div>`;
}
function renderMotionProperty(p,part){const motion=p.motions.find(m=>m.id===part.motionId);
  return `<div class="property-section"><div class="prop-head">MOTION ASSIGNMENT</div><label class="prop-label">Animation</label><select id="part-motion" class="prop-select"><option value="">Static (no animation)</option>${p.motions.map(m=>`<option value="${escapeHTML(m.id)}" ${part.motionId===m.id?'selected':''}>${escapeHTML(m.name)}</option>`).join('')}</select><button class="outline-btn field-button" data-action="add-motion">＋ New motion</button></div>`+
  (motion?`<div class="property-section"><div class="prop-head">ANIMATION PARAMETERS</div><label class="prop-label">Name</label><input class="prop-input" data-motion-text="name" value="${escapeHTML(motion.name)}"/><label class="prop-label">Type</label><select class="prop-select" data-motion-choice="type"><option value="spin" ${motion.type==='spin'?'selected':''}>Rotation</option><option value="slide" ${motion.type==='slide'?'selected':''}>Oscillation / slide</option></select><label class="prop-label">Axis</label><select class="prop-select" data-motion-choice="axis">${['x','y','z'].map(x=>`<option ${motion.axis===x?'selected':''} value="${x}">${x.toUpperCase()}</option>`).join('')}</select><label class="prop-label">RPM / cycles per minute</label><input type="number" data-motion-number="rpm" step="1" class="prop-input" value="${motion.rpm||12}"/>${motion.type==='slide'?'<label class="prop-label">Travel amplitude (blocks)</label><input type="number" data-motion-number="amplitude" step="0.0625" class="prop-input" value="'+(motion.amplitude||.15)+'"/>':''}<label class="prop-label">Rotation / motion pivot</label>${coordFields('pivot',motion.pivot||part.from.map((x,i)=>(x+part.to[i])/2))}<button class="outline-btn danger field-button" data-action="remove-motion">Delete motion</button><p class="prop-help">Animation previews are real-time in Studio, but the mod runtime adapter has not been implemented yet.</p></div>`:'');
}
function roleGlyph(role){return ({Controller:'C',Frame:'▧',MechanicalInput:'M',ItemInput:'I',ItemOutput:'O',FluidInput:'F',FluidOutput:'F',PrimaryOutput:'P',ByproductOutput:'B'})[role]||role?.slice(0,1)?.toUpperCase()||'';}
function renderDock(){const p=state.project;
  if(state.mode==='build'){
    $('dock-actions').innerHTML=`<button class="little-action" data-action="fill-border">□ Frame border</button><button class="little-action" data-action="clear-layer">× Clear layer</button>`;
    const [w,,d]=p.dimensions,cellSize=w>32||d>32?16:w>18||d>18?21:29;
    const html=[];
    for(let z=0;z<d;z++) for(let x=0;x<w;x++){
      const key=cellKey(x,state.layer,z),role=p.cells[key]||'Air',mat=materialById(materialForBlock(p.palette[role]));
      html.push(`<button class="grid-cell ${role==='Air'?'is-air':''} ${state.selectedCell===key?'selected':''}" data-cell="${key}" title="${x}, ${state.layer}, ${z}: ${escapeHTML(role)}" style="width:${cellSize}px;height:${cellSize}px;${role==='Air'?'':`background:${mat.color}45;border-color:${mat.color}82;`}">${role==='Air'?'':escapeHTML(roleGlyph(role))}</button>`);
    }
    $('layer-grid').innerHTML=`<div class="matrix" style="grid-template-columns:repeat(${w},${cellSize}px)">${html.join('')}</div>`;
    $('dock-side').innerHTML=`<div class="dock-side-head">LAYER ${state.layer} / ${p.dimensions[1]-1}</div><p>Painting <strong>${escapeHTML(state.selectedRole)}</strong><br>Grid X →, Z ↓. Left click / drag to paint.</p><div class="readout">${w} × ${d} blocks</div><p>Select <strong>Air</strong> to erase. A cell left empty in the editor exports as required air in the legacy format.</p>`;
  }else{
    $('dock-actions').innerHTML=state.mode==='motion'?`<button class="little-action" data-action="toggle-play">${state.playing?'Ⅱ Pause':'▶ Play'}</button><button class="little-action" data-action="reset-animation">↺ Reset</button>`:`<button class="little-action" data-action="add-part">＋ Add cuboid</button><button class="little-action" data-action="add-frame">▧ Frame preset</button>`;
    renderPlan();
  }
}
function renderPlan(){const p=state.project,[w,,d]=p.dimensions, cellPx=clamp(170/Math.max(w,d),3,27), width=w*cellPx,height=d*cellPx;
  const grid=[];
  for(let x=0;x<=w;x++)grid.push(`<line x1="${x*cellPx}" x2="${x*cellPx}" y1="0" y2="${height}" stroke="#3a5361" stroke-width=".6"/>`);
  for(let z=0;z<=d;z++)grid.push(`<line y1="${z*cellPx}" y2="${z*cellPx}" x1="0" x2="${width}" stroke="#3a5361" stroke-width=".6"/>`);
  for(const part of p.parts){if(part.visible===false)continue;
    let [x,,z]=part.from,[X,,Z]=part.to;const selected=state.selectedPart===part.id;
    grid.push(`<rect data-plan-part="${escapeHTML(part.id)}" x="${x*cellPx}" y="${z*cellPx}" width="${Math.max(1,(X-x)*cellPx)}" height="${Math.max(1,(Z-z)*cellPx)}" rx="1" fill="${materialById(part.material).color}" fill-opacity="${selected?.85:.55}" stroke="${selected?'#ffcb94':'#121e27'}" stroke-width="${selected?2:1}" style="cursor:move"/>`);
  }
  $('layer-grid').innerHTML=`<svg id="model-plan" viewBox="-6 -6 ${width+12} ${height+12}" width="${width+12}" height="${height+12}" style="max-width:96%;max-height:175px;overflow:visible;touch-action:none" aria-label="Top-down part drag editor">${grid.join('')}<text x="0" y="-1" font-size="4" fill="#84adca">Z ↓</text></svg>`;
  const part=p.parts.find(part=>part.id===state.selectedPart);
  $('dock-side').innerHTML=`<div class="dock-side-head">${state.mode==='motion'?'ANIMATION PREVIEW':'TOP-DOWN MODEL PLAN'}</div><p>${part?`Selected: <strong>${escapeHTML(part.name)}</strong><br/>Drag its rectangle to move in X/Z.`:'Select any cube in the plan or 3D viewport.'}</p><p>${state.mode==='motion'?'Movement is preview-only until game runtime integration.':'Model coordinates snap to 1/16 of a Minecraft block.'}</p><div class="readout">${w} × ${d} BLOCK FOOTPRINT</div>`;
  const svg=$('model-plan');let drag=null;
  const positionOf=e=>{const point=svg.createSVGPoint();point.x=e.clientX;point.y=e.clientY;const m=svg.getScreenCTM();if(!m)return {x:0,z:0};const q=point.matrixTransform(m.inverse());return {x:q.x/cellPx,z:q.y/cellPx};};
  svg.addEventListener('pointerdown',e=>{const id=e.target?.dataset?.planPart;if(!id)return;
    const pt=p.parts.find(pt=>pt.id===id);if(!pt)return;
    state.selectedPart=id;const start=positionOf(e);drag={id,start,from:[...pt.from],to:[...pt.to]};svg.setPointerCapture(e.pointerId);renderInspector();renderSidebar();viewport.schedule();e.preventDefault();
  });
  svg.addEventListener('pointermove',e=>{if(!drag)return;const pt=p.parts.find(pt=>pt.id===drag.id);if(!pt)return;const pos=positionOf(e),dx=snap(pos.x-drag.start.x),dz=snap(pos.z-drag.start.z);pt.from=[drag.from[0]+dx,drag.from[1],drag.from[2]+dz];pt.to=[drag.to[0]+dx,drag.to[1],drag.to[2]+dz];viewport.schedule();
    const r=svg.querySelector(`[data-plan-part="${CSS.escape(drag.id)}"]`);if(r){r.setAttribute('x',pt.from[0]*cellPx);r.setAttribute('y',pt.from[2]*cellPx);}
  });
  const endDrag=()=>{if(!drag)return;drag=null;changed('MODEL PART MOVED');};svg.addEventListener('pointerup',endDrag);svg.addEventListener('pointercancel',endDrag);
}
function setMode(mode){if(!['build','model','motion'].includes(mode))return;state.mode=mode;state.playing=false;refresh();}
function paintCell(x,y,z){const p=state.project;if([x,y,z].some((v,i)=>v<0||v>=p.dimensions[i]))return;
  const key=cellKey(x,y,z);if(state.selectedRole==='Air')delete p.cells[key];else p.cells[key]=state.selectedRole;state.selectedCell=key;changed('CONSTRUCTION UPDATED');
}
function setProject(p,message='PROJECT LOADED'){state.project=normalizeProject(p);state.layer=0;state.mode='build';state.selectedCell=null;state.selectedPart=null;state.selectedRole=Object.keys(state.project.palette).includes('Frame')?'Frame':Object.keys(state.project.palette).find(x=>x!=='Air')||'Air';state.playing=false;state.dirty=false;try{localStorage.setItem(autosaveKey,JSON.stringify(state.project));}catch{}refresh();viewport.resetFit();report(message);}
function addPart({name='New cuboid',from,to,material='wood',group='Static',motionId=null}={}){
  const [w,h,d]=state.project.dimensions;const x=Math.floor(w/2),z=Math.floor(d/2);
  const part={id:uid(),name,from:from||[x,0,z],to:to||[x+1,1,z+1],material,group,visible:true};if(motionId)part.motionId=motionId;
  state.project.parts.push(part);state.selectedPart=part.id;state.mode='model';changed('CUBOID ADDED');return part;
}
function makeFrame(){const [w,h,d]=state.project.dimensions;if(w<1||h<1||d<1)return;
  const width=Math.min(.25,w/3,d/3),y=.20,parts=[];
  for(const x of [0,w-width])for(const z of [0,d-width])parts.push({name:'Frame post',from:[x,0,z],to:[x+width,h,z+width]});
  for(const height of [y,Math.max(y,h-.3)]){
    for(const z of [0,d-width])parts.push({name:'Long beam',from:[0,height,z],to:[w,height+width,z+width]});
    for(const x of [0,w-width])parts.push({name:'Cross beam',from:[x,height,0],to:[x+width,height+width,d]});
  }
  for(const part of parts)state.project.parts.push({id:uid(),...part,material:'wood',group:'Generated frame',visible:true});
  state.selectedPart=state.project.parts.at(-1)?.id;state.mode='model';changed('TIMBER FRAME GENERATED');report('Generated timber frame geometry');
}
function makeModelsFromStructure(){const entries=Object.entries(state.project.cells).filter(([,v])=>v!=='Air');if(entries.length>1000){report('Too many cells for one-cuboid-per-block preview (maximum 1000)',true);return;}
  for(const [key,role] of entries){const [x,y,z]=coordsOf(key);state.project.parts.push({id:uid(),name:`${role} [${x},${y},${z}]`,from:[x,y,z],to:[x+1,y+1,z+1],group:'Imported construction',material:materialForBlock(state.project.palette[role]),visible:true});}
  state.mode='model';changed('GEOMETRY CREATED FROM CONSTRUCTION');report(`Created ${entries.length} editable cuboids`);
}
function addMotion(){const part=state.project.parts.find(pt=>pt.id===state.selectedPart);if(!part){report('Select a cuboid before adding its motion',true);return;}
  const pivot=part.from.map((a,i)=>snap((a+part.to[i])/2));const m={id:uid(),name:`${part.name} rotation`,type:'spin',axis:'y',rpm:12,amplitude:.15,pivot};state.project.motions.push(m);part.motionId=m.id;state.mode='motion';changed('MOTION CREATED');report('Motion added to selected cuboid');
}
function currentPart(){return state.project.parts.find(x=>x.id===state.selectedPart);}
function currentMotion(){return state.project.motions.find(x=>x.id===currentPart()?.motionId);}
function downloadFile(filename,content,mime='application/json'){const blob=content instanceof Blob?content:new Blob([content],{type:mime}),url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download=filename;document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),5000);}
function saveProject(){downloadFile(`${state.project.id}.mcraftstudio.json`,JSON.stringify(state.project,null,2));state.dirty=false;report('Editable Studio project saved');}
function exportProject(){try{const files=compileExportFiles(state.project),zip=createZip(files),blob=new Blob([zip],{type:'application/zip'});downloadFile(`${state.project.id}-forge-export.zip`,blob,'application/zip');const count=Object.keys(files).length;report(`Exported ${count} files. Mod-side tiled renderer integration still required.`);}catch(err){report(`Export failed: ${err.message}`,true);console.error(err);}}
async function loadJsonFile(file){try{const text=await file.text(),json=JSON.parse(text);const imported=json?.structure ?importLegacyStructure(json):normalizeProject(json);setProject(imported,json?.structure?'Magneticraft construction JSON imported':'Studio project opened');if(json?.structure)report('Structure imported; original formed visuals are not included in construction JSON');}catch(err){report(`Unable to open: ${err.message}`,true);}}
async function loadSample(which){try{const resp=await fetch(`samples/${which}_north.json`);if(!resp.ok)throw Error('Sample file unavailable');setProject(importLegacyStructure(await resp.json()),`${which==='mechanical_ore_washer'?'Ore Washer':'Sifter'} structure imported`);}catch(err){report(err.message,true);}}
function onAction(action,value){const p=state.project;
  if(action==='sample')loadSample(value);
  else if(action==='mode')setMode(value);
  else if(action==='role'){state.selectedRole=value;refresh();}
  else if(action==='select-part'){state.selectedPart=value;refresh();}
  else if(action==='layer-prev'){state.layer=clamp(state.layer-1,0,p.dimensions[1]-1);refresh();}
  else if(action==='layer-next'){state.layer=clamp(state.layer+1,0,p.dimensions[1]-1);refresh();}
  else if(action==='add-part')addPart();
  else if(action==='add-frame')makeFrame();
  else if(action==='make-block-models')makeModelsFromStructure();
  else if(action==='duplicate-part'){const part=currentPart();if(part){const cp=clone(part);cp.id=uid();cp.name=part.name+' copy';cp.from[0]+=0.25;cp.to[0]+=0.25;p.parts.push(cp);state.selectedPart=cp.id;changed('CUBOID DUPLICATED');}}
  else if(action==='remove-part'){if(!currentPart())return;p.parts=p.parts.filter(pt=>pt.id!==state.selectedPart);state.selectedPart=null;changed('CUBOID REMOVED');}
  else if(action==='erase-cell'){if(state.selectedCell){delete p.cells[state.selectedCell];changed('CELL ERASED');}}
  else if(action==='add-role'){
    const name=window.prompt('New role name (e.g. HeatInput):');if(!name)return;
    const role=name.trim();if(!/^[A-Za-z][A-Za-z0-9_]*$/.test(role)||p.palette[role]){report('Role must be unique and contain only letters, digits or underscores',true);return;}
    const id=window.prompt('Minecraft block ID:',`magneticraft2:${role.toLowerCase()}`);if(!id)return;p.palette[role]=id.trim();state.selectedRole=role;changed('ROLE ADDED');
  }
  else if(action==='clear-layer'){if(!window.confirm(`Clear every construction block on layer ${state.layer}?`))return;for(const key of Object.keys(p.cells)){const [x,y,z]=coordsOf(key);if(y===state.layer)delete p.cells[key];}changed('LAYER CLEARED');}
  else if(action==='fill-border') {const [w,,d]=p.dimensions;for(let x=0;x<w;x++)for(let z=0;z<d;z++)if(x===0||z===0||x===w-1||z===d-1){const key=cellKey(x,state.layer,z);if(state.selectedRole==='Air')delete p.cells[key];else p.cells[key]=state.selectedRole;}changed('BORDER FILLED');}
  else if(action==='resize'){
    const dims=[0,1,2].map(i=>Number($(`dim-${i}`).value));if(dims.some(n=>!Number.isInteger(n)||n<1||n>128)){report('Dimensions must be whole numbers between 1 and 128',true);return;}
    const lost=Object.keys(p.cells).filter(key=>coordsOf(key).some((n,i)=>n>=dims[i]));
    if(lost.length&&!window.confirm(`Resize will remove ${lost.length} out-of-range construction cells. Continue?`))return;
    for(const key of lost)delete p.cells[key];p.dimensions=dims;state.layer=clamp(state.layer,0,dims[1]-1);changed('STRUCTURE RESIZED');viewport.resetFit();
  }
  else if(action==='add-motion')addMotion();
  else if(action==='remove-motion'){const m=currentMotion();if(!m)return;p.motions=p.motions.filter(x=>x.id!==m.id);p.parts.forEach(pt=>{if(pt.motionId===m.id)delete pt.motionId;});changed('MOTION REMOVED');}
  else if(action==='select-motion'){const pt=p.parts.find(pt=>pt.motionId===value);if(pt){state.selectedPart=pt.id;refresh();}}
  else if(action==='toggle-play'){state.playing=!state.playing;refresh();}
  else if(action==='reset-animation'){state.playing=false;viewport.animTime=0;refresh();}
}
const viewport=new StudioViewport($('viewport'),{getState:()=>state,onPaint:paintCell,onSelectPart:id=>{state.selectedPart=id;refresh();}});
function showNewDialog(){$('new-name').value='New Machine';$('new-modal').showModal();}
$('new-form').addEventListener('submit',e=>{e.preventDefault();const dims=[$('new-x'),$('new-y'),$('new-z')].map(x=>Number(x.value));if(dims.some(n=>!Number.isInteger(n)||n<1||n>128)){report('Dimensions must be between 1 and 128',true);return;}setProject(makeProject($('new-name').value||'New Machine',dims),'New multiblock created');$('new-modal').close();});
$('new-close').addEventListener('click',()=>$('new-modal').close());$('new-cancel').addEventListener('click',()=>$('new-modal').close());
$('btn-new').addEventListener('click',showNewDialog);$('btn-open').addEventListener('click',()=>$('file-open').click());$('btn-save').addEventListener('click',saveProject);$('btn-export').addEventListener('click',exportProject);$('btn-fit').addEventListener('click',()=>viewport.resetFit());
$('file-open').addEventListener('change',e=>{if(e.target.files?.[0])loadJsonFile(e.target.files[0]);e.target.value='';});
$('toggle-all').addEventListener('change',e=>{state.showAll=e.target.checked;viewport.schedule();});$('toggle-ghost').addEventListener('change',e=>{state.ghostBuild=e.target.checked;viewport.schedule();});
document.querySelectorAll('.mode-tab').forEach(el=>el.addEventListener('click',()=>setMode(el.dataset.mode)));
let painting=false;
$('layer-grid').addEventListener('pointerdown',e=>{const cell=e.target.closest('[data-cell]');if(!cell)return;e.preventDefault();painting=true;const pos=coordsOf(cell.dataset.cell);paintCell(...pos);});
$('layer-grid').addEventListener('pointerover',e=>{if(!painting)return;const cell=e.target.closest('[data-cell]');if(!cell)return;const pos=coordsOf(cell.dataset.cell);if(state.selectedCell!==cell.dataset.cell)paintCell(...pos);});
window.addEventListener('pointerup',()=>painting=false);
for(const side of ['left-sidebar','right-sidebar','dock-actions']) $(side).addEventListener('click',e=>{const button=e.target.closest('[data-action]');if(button)onAction(button.dataset.action,button.dataset.value);});
$('left-sidebar').addEventListener('input',e=>{if(e.target.id==='layer-slider'){state.layer=Number(e.target.value);refresh();}});
$('right-sidebar').addEventListener('change',e=>{
  const t=e.target,p=state.project,part=currentPart();
  if(t.id==='brush-role'){state.selectedRole=t.value;refresh();return;}
  if(t.id==='palette-block'){if(!/^[a-z0-9_.-]+:[a-z0-9_./-]+$/.test(t.value)){report('Use namespace:block_id for registry IDs',true);return;}p.palette[state.selectedRole]=t.value.trim();changed('BLOCK ROLE MAPPING UPDATED');return;}
  if(t.dataset.setting){p.settings[t.dataset.setting]=t.checked;changed('MACHINE SETTINGS UPDATED');return;}
  if(!part)return;
  if(t.dataset.partToggle){part[t.dataset.partToggle]=t.checked;changed('PART VISIBILITY UPDATED');return;}
  if(t.dataset.partText){part[t.dataset.partText]=t.value;changed('PART UPDATED');return;}
  if(t.dataset.partVec==='pivot'){const m=currentMotion();if(!m)return;if(!m.pivot)m.pivot=[0,0,0];m.pivot[Number(t.dataset.axis)]=snap(Number(t.value));changed('MOTION PIVOT UPDATED');return;}
  if(t.dataset.partVec){let v=Number(t.value);if(!Number.isFinite(v)){report('Invalid coordinate',true);return;}part[t.dataset.partVec][Number(t.dataset.axis)]=snap(v);changed('PART POSITION UPDATED');return;}
  if(t.id==='part-material'){part.material=t.value;delete part.texture;changed('PART MATERIAL UPDATED');return;}
  if(t.id==='part-texture'){if(!/^[a-z0-9_.-]+:[a-z0-9_./-]+$/.test(t.value.trim())){report('Texture must use namespace:path',true);return;}part.texture=t.value.trim();changed('PART TEXTURE UPDATED');return;}
  if(t.id==='part-motion'){if(t.value)part.motionId=t.value;else delete part.motionId;changed('MOTION ASSIGNMENT UPDATED');return;}
  const m=currentMotion();if(!m)return;
  if(t.dataset.motionText){m[t.dataset.motionText]=t.value;changed('MOTION UPDATED');return;}
  if(t.dataset.motionChoice){m[t.dataset.motionChoice]=t.value;changed('MOTION UPDATED');return;}
  if(t.dataset.motionNumber){m[t.dataset.motionNumber]=Number(t.value)||0;changed('MOTION UPDATED');return;}
});
window.addEventListener('keydown',e=>{
  const ctrl=e.ctrlKey||e.metaKey;if(ctrl&&e.key.toLowerCase()==='s'){e.preventDefault();saveProject();return;}
  const tag=e.target?.tagName;if(['INPUT','TEXTAREA','SELECT'].includes(tag))return;
  if(e.key==='Escape'){state.selectedPart=null;state.selectedCell=null;refresh();return;}
  if(e.key.toLowerCase()==='f')viewport.resetFit();
  if(e.key.toLowerCase()==='b')setMode('build');
  if(e.key.toLowerCase()==='m')setMode('model');
  if(e.key.toLowerCase()==='a')setMode('motion');
  if(e.key.toLowerCase()==='g'&&state.mode!=='build')addPart();
  if(e.key==='Delete'&&state.mode!=='build')onAction('remove-part');
});
refresh();viewport.resetFit();
window.__studio_debug={state,viewport,makeLegacy:()=>compileExportFiles(state.project),loadSample,setProject,addPart};