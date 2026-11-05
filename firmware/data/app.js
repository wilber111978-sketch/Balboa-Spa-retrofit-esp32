import {runtime} from './runtime.js';
import {DemoSpaService,HttpSpaService} from './services.js';
import {clone,controlsBlocked,isStale,LIMITS,validateState,nextTarget} from './model.js';
import {icon,renderPage,renderBanners,renderDemoBar,faultDetail,escapeHTML} from './views.js';
const live=runtime.mode==='live';
const service=live?new HttpSpaService({requireCapabilities:true}):new DemoSpaService();
const vm={live,offline:false,capabilities:null,state:null,events:[],diagnostics:{},page:'home',filter:'all',scenario:'heating',updated:0,busy:false,paused:false,rejectNext:false,scheduleDraft:null};
const main=document.querySelector('#main'),banners=document.querySelector('#banners'),feedback=document.querySelector('#feedback'),dialog=document.querySelector('#detail-dialog');
let revision=0,confirmAction=null,lastBlocked=null;
function apply(bundle){validateState(bundle.state);vm.offline=false;vm.capabilities=bundle.capabilities||null;vm.state=bundle.state;vm.events=bundle.events;vm.diagnostics=bundle.diagnostics;if(vm.state.controller.online)vm.updated=Date.now();}
function message(text,error=false){feedback.textContent=text;feedback.classList.toggle('error',error);}
function render(force=true){if(!vm.state)return;
  const blocked=vm.offline||controlsBlocked(vm.state,vm.updated)||vm.busy;
  banners.innerHTML=renderBanners(vm);
  const connection=document.querySelector('#connection');connection.textContent=vm.offline||!vm.state.controller.online?'Offline':isStale(vm.updated)?'Stale data':live?'Controller online':'Demo online';connection.classList.toggle('disconnected',vm.offline||!vm.state.controller.online||isStale(vm.updated));
  main.setAttribute('aria-busy',String(vm.busy));
  if(force||blocked!==lastBlocked){
    const active=document.activeElement;const action=active?.dataset?.action,value=active?.dataset?.value;
    main.innerHTML=renderPage(vm);
    if(action){const replacement=[...main.querySelectorAll('[data-action]')].find(el=>el.dataset.action===action&&el.dataset.value===value);replacement?.focus({preventScroll:true});}
  }
  lastBlocked=blocked;
  const nav=document.querySelector('.bottom-nav');
  if(nav.dataset.page!==vm.page){nav.dataset.page=vm.page;nav.innerHTML=['home','history','diagnostics','settings'].map(page=>`<button data-action="navigate" data-page="${page}" ${vm.page===page?'aria-current="page"':''}>${icon(page)}<span>${page[0].toUpperCase()+page.slice(1)}</span></button>`).join('');}
  const select=document.querySelector('#scenario');if(select)select.disabled=vm.busy;
}
function navigate(page){if(!['home','history','diagnostics','settings'].includes(page))return;vm.page=page;render();main.focus({preventScroll:true});window.scrollTo({top:0,behavior:'instant'});}
async function command(kind,payload){
  if(vm.busy)return;
  const exceptional=kind==='service-mode'||kind==='fault-reset';
  if(vm.offline||(!exceptional&&controlsBlocked(vm.state,vm.updated))||!vm.state.controller.online||isStale(vm.updated)){message('Request not sent: controller state is offline, stale or locked.',true);return;}
  revision++;vm.busy=true;message('Request pending…');render();
  try{
    if(vm.rejectNext){vm.rejectNext=false;throw new Error('Simulated API rejection. Request was not applied.');}
    apply(await service.request(kind,payload));vm.scheduleDraft=null;message(live?'Controller accepted the request. Actual state refreshed.':'Request accepted by demo controller.');
  }catch(error){message('Request not confirmed: '+error.message,true);try{apply(await service.read());}catch{vm.offline=true;/* Keep last known values. */}}
  finally{vm.busy=false;render();}
}
function openDialog(title,html,action=null){document.querySelector('#dialog-content').innerHTML=`<h2 id="dialog-title">${escapeHTML(title)}</h2>${html}`;confirmAction=action;document.querySelector('#dialog-confirm').hidden=!action;document.querySelector('#dialog-cancel').textContent=action?'Cancel':'Close';dialog.showModal();}
document.querySelector('#dialog-cancel').addEventListener('click',()=>{confirmAction=null;dialog.close();});
document.querySelector('#dialog-confirm').addEventListener('click',()=>{const action=confirmAction;confirmAction=null;dialog.close();action?.();});
dialog.addEventListener('cancel',()=>{confirmAction=null;});
document.querySelector('#open-settings').addEventListener('click',()=>navigate('settings'));
document.addEventListener('click',event=>{
  const control=event.target.closest('[data-action]');if(!control||control.disabled)return;
  const action=control.dataset.action;
  if(action==='navigate')navigate(control.dataset.page);
  else if(action==='target-up'||action==='target-down')command('target',{target_c:nextTarget(vm.state.temperature.target_c,action==='target-up'?1:-1)});
  else if(action==='jets')command('jets',{requested:control.dataset.value});
  else if(action==='light')command('light',{on:!vm.state.light.on});
  else if(action==='mode')command('mode',{heating:control.dataset.value});
  else if(action==='filter'){vm.filter=control.dataset.value;render();}
  else if(action==='event')openDialog('Event detail',faultDetail(vm.events[Number(control.dataset.index)]));
  else if(action==='confirm-service')openDialog(vm.state.mode.service?'Exit Service Mode?':live?'Stop all controller outputs?':'Stop all simulated outputs?',`<p>${vm.state.mode.service?'Automatic controller operation may resume after exit.':live?'Persistent controller Service Mode stops output commands and suspends schedules.':'Service Mode persists in this browser until explicitly exited. All simulated outputs stop and schedules are suspended.'}</p><p class="muted">${live?'This is a real controller request, not electrical isolation.':'This demo does not control hardware or provide electrical isolation.'}</p>`,()=>command('service-mode',{enabled:!vm.state.mode.service,confirm:true}));
  else if(action==='confirm-reset')openDialog('Acknowledge cleared fault?','<p>The controller must confirm the cause is cleared. This does not exit Service Mode.</p>',()=>command('fault-reset',{confirm:true}));
  else if(action==='pause'){vm.paused=!vm.paused;document.querySelector('#demo-bar').innerHTML=renderDemoBar(vm);message(vm.paused?'Updates paused. Values become stale after six seconds.':'Updates resumed.');}
  else if(action==='reject'){vm.rejectNext=true;message('The next request will be rejected without applying it.');}
});
document.addEventListener('change',async event=>{
  if(event.target.id!=='scenario')return;
  if(vm.busy)return;revision++;vm.busy=true;render();
  try{apply(await service.selectScenario(event.target.value));vm.scenario=event.target.value;vm.scheduleDraft=null;vm.paused=false;message('Demo scenario loaded. No physical outputs are connected.');}
  catch(error){message(error.message,true);}finally{vm.busy=false;document.querySelector('#demo-bar').innerHTML=renderDemoBar(vm);render();}
});
function scheduleDraft(form){return {enabled:live?form.elements.start.value!==form.elements.end.value:form.elements.enabled.checked,start:form.elements.start.value,end:form.elements.end.value,days:[...form.querySelectorAll('[name=day]:checked')].map(el=>Number(el.value))};}
document.addEventListener('input',event=>{const form=event.target.closest('#schedule-form');if(form)vm.scheduleDraft=clone(scheduleDraft(form));});
document.addEventListener('submit',event=>{if(event.target.id!=='schedule-form')return;event.preventDefault();const draft=scheduleDraft(event.target);vm.scheduleDraft=draft;command('schedule',draft);});
async function poll(){if(vm.busy||vm.paused){render(false);return;}const ticket=revision;
  try{const bundle=await service.read();if(ticket!==revision)return;const changed=JSON.stringify(bundle)!==JSON.stringify({state:vm.state,events:vm.events,diagnostics:vm.diagnostics});apply(bundle);render(changed&&!document.activeElement.closest('#schedule-form'));}
  catch(error){vm.offline=true;message('Controller refresh failed: '+error.message,true);render(false);}
}
try{apply(await service.read());document.querySelector('#demo-bar').innerHTML=live?'':renderDemoBar(vm);document.querySelector('#demo-bar').hidden=live;document.querySelector('.app-footer').textContent=live?'Live controller requests. Physical heater outputs remain inhibited.':'Demo only. Simulated heater states never control hardware.';render();setInterval(poll,1500);setInterval(()=>render(false),1000);}catch(error){main.textContent='Unable to start the interface: '+error.message;}
