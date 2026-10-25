import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {makeProject,makeDemo,cellKey,importLegacyStructure,makeLegacyStructure,sliceStaticParts,compileExportFiles,rotateCell,rotateBox} from '../src/core.mjs';
import {createZip,crc32} from '../src/zip.mjs';
const source=JSON.parse(readFileSync(new URL('../samples/mechanical_ore_washer_north.json', import.meta.url),'utf8'));
const sifter=JSON.parse(readFileSync(new URL('../samples/mechanical_sifter_north.json', import.meta.url),'utf8'));
test('import and export of existing Ore Washer construction is exact',()=>{
 const imported=importLegacyStructure(source),generated=makeLegacyStructure(imported,'north');
 assert.deepEqual(generated.structure,source.structure);
 assert.equal(generated.settings.replacewhenformed,source.settings.replacewhenformed);
 assert.deepEqual(imported.dimensions,[3,2,3]);
 assert.equal(imported.cells['1,0,1'],'Controller');
});
test('Sifter import and structure roundtrip',()=>{
 const imported=importLegacyStructure(sifter),generated=makeLegacyStructure(imported,'north');
 assert.deepEqual(generated.structure,sifter.structure);
 assert.equal(imported.cells['0,0,0'],'ByproductOutput');
});
test('90° rotations follow existing Magneticraft directional layout',()=>{
 const p=importLegacyStructure(source),east=makeLegacyStructure(p,'east');
 assert.deepEqual(east.structure.layout.layer1,[
  ['Frame','FluidOutput','Frame'],
  ['ItemInput','Controller','ItemOutput'],
  ['Frame','FluidInput','Frame']
 ]);
 assert.deepEqual(rotateCell(1,0,[5,3,9],'east'),[8,1]);
 assert.deepEqual(rotateBox([1,0,2],[3,1,6],[5,3,9],'east'),{from:[3,0,1],to:[7,1,3]});
});
test('importing an east-facing legacy file normalizes it back to north',()=>{
 const north=importLegacyStructure(source);
 const east=makeLegacyStructure(north,'east');
 const roundtrip=importLegacyStructure(east);
 assert.deepEqual(roundtrip.dimensions,north.dimensions);
 assert.deepEqual(roundtrip.cells,north.cells);
 assert.deepEqual(makeLegacyStructure(roundtrip,'north').structure.layout, source.structure.layout);
});
test('importing a rectangular west-facing structure swaps footprint back correctly',()=>{
 const p=makeProject('Rectangular',[7,3,11]);
 p.cells[cellKey(0,0,0)]='Controller';p.cells[cellKey(5,2,10)]='Frame';
 const west=makeLegacyStructure(p,'west');const loaded=importLegacyStructure(west);
 assert.deepEqual(loaded.dimensions,[7,3,11]);
 assert.deepEqual(loaded.cells,p.cells);
});
test('rectangular footprint transforms dimensions east/west',()=>{
 const p=makeProject('Big Factory',[16,12,20]);
 assert.deepEqual(makeLegacyStructure(p,'east').structure.dimensions,[20,12,16]);
 assert.deepEqual(makeLegacyStructure(p,'west').structure.dimensions,[20,12,16]);
});
test('static parts are split and clipped to valid Minecraft model coordinates',()=>{
 const p=makeProject('Large Machine',[5,3,5]);
 p.parts.push({id:'big',name:'Big box',from:[0,0,0],to:[3,2,3],material:'wood',group:'static',visible:true});
 const {models,warnings}=sliceStaticParts(p);
 assert.equal(Object.keys(models).length,18);
 assert.deepEqual(warnings,[]);
 for(const model of Object.values(models))for(const el of model.elements){
  for(const v of [...el.from,...el.to]) assert.ok(v>=0&&v<=16);
  assert.ok(el.faces && Object.keys(el.faces).length>0);
 }
 assert.equal(Object.keys(models['1,0,1'].elements[0].faces).join(','),'down');
 assert.equal(Object.keys(models['1,1,1'].elements[0].faces).join(','),'up');
});
test('large split model 16x12x20 exports with four rotations',()=>{
 const p=makeProject('Big Factory',[16,12,20]);
 p.id='big_factory';p.parts.push({id:'roof',name:'Roof',group:'Static',from:[0,11,0],to:[16,12,20],material:'iron',visible:true});
 p.cells[cellKey(8,0,10)]='Controller';
 const files=compileExportFiles(p);
 assert.ok(files['src/main/resources/data/magneticraft2/multiblocks/big_factory_east.json']);
 const names=Object.keys(files).filter(s=>s.includes('/models/block/studio/big_factory/north/'));
 assert.equal(names.length,320);
 const manifest=JSON.parse(files['src/main/resources/assets/magneticraft2/studio/big_factory_manifest.json']);
 assert.equal(Object.keys(manifest.models.east.cells).length,320);
});
test('mod compatibility caveat explicitly included in export',()=>{
 const files=compileExportFiles(makeDemo());
 assert.match(files['EXPORT_README.txt'],/REQUIRES new code in the mod/);
 assert.match(files['EXPORT_README.txt'],/EDITOR PREVIEW METADATA ONLY/);
});
test('zip header checksums and entries',()=>{
 const z=createZip({'alpha.txt':'hello','beta.txt':'world'});
 assert.deepEqual([...z.slice(0,4)],[0x50,0x4b,3,4]);
 assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926);
 writeFileSync(new URL('./test-export.zip', import.meta.url),createZip(compileExportFiles(makeDemo())));
});
