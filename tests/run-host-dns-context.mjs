// SPDX-License-Identifier: Apache-2.0
// Run: node tests/run-host-dns-context.mjs [--baseline NetworkManager.cpp] [--output EXECUTABLE]
import {readFileSync,mkdirSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
const here=dirname(fileURLToPath(import.meta.url)),root=resolve(here,'..'),options={};
for(let i=2;i<process.argv.length;i+=2){
  if(!['--baseline','--output'].includes(process.argv[i])||!process.argv[i+1])throw Error('Use optional --baseline SOURCE and --output EXECUTABLE');
  options[process.argv[i]]=resolve(process.argv[i+1]);
}
const out=options['--output']||resolve(root,'build/validation/host-dns-context'+(process.platform==='win32'?'.exe':''));
mkdirSync(dirname(out),{recursive:true});
const harness=readFileSync(resolve(here,'host_by_name_dns_context.cpp'),'utf8');
for(const baseline of options['--baseline']?[true,false]:[false])for(const ipv6 of [0,1]){
  const source=readFileSync(baseline?options['--baseline']:resolve(root,'libraries/Network/src/NetworkManager.cpp'),'utf8');
  const start=source.indexOf('int NetworkManager::hostByName('),end=source.indexOf('uint8_t *NetworkManager::macAddress(',start);
  if(start<0||end<0)throw Error('Actual hostByName boundaries changed');
  const digest=createHash('sha256').update(source).digest('hex');
  if(baseline&&digest!=='a4c0c9c35fa185884d5acbc3c9a3e029d0fb9e4bf824d313e81b5dd4c6d5328a')throw Error('Baseline is not reviewed upstream3e66933 source');
  console.log((baseline?'BASELINE':'PATCHED')+' ipv6='+ipv6+' sha256='+digest);
  const result=spawnSync(process.env.CXX||'g++',['-std=c++17','-Wall','-Wextra','-Werror','-O0','-g0','-DCONFIG_LWIP_IPV6='+ipv6,'-x','c++','-','-o',out],
      {input:harness.replace('// ACTUAL_IMPLEMENTATION',()=>source.slice(start,end)),encoding:'utf8',timeout:30000,windowsHide:true});
  if(result.error||result.status!==0)throw Error('Actual hostByName fixture compile failed: '+result.stderr);
  const run=spawnSync(out,[],{encoding:'utf8',timeout:10000,windowsHide:true});console.log('exit='+run.status+' '+run.stdout+run.stderr);
  if(run.error||run.status!==(baseline?1:0))throw Error('Unexpected baseline/patch DNS result');
}
