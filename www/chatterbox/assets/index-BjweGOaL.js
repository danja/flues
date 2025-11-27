(function(){const t=document.createElement("link").relList;if(t&&t.supports&&t.supports("modulepreload"))return;for(const i of document.querySelectorAll('link[rel="modulepreload"]'))s(i);new MutationObserver(i=>{for(const n of i)if(n.type==="childList")for(const a of n.addedNodes)a.tagName==="LINK"&&a.rel==="modulepreload"&&s(a)}).observe(document,{childList:!0,subtree:!0});function e(i){const n={};return i.integrity&&(n.integrity=i.integrity),i.referrerPolicy&&(n.referrerPolicy=i.referrerPolicy),i.crossOrigin==="use-credentials"?n.credentials="include":i.crossOrigin==="anonymous"?n.credentials="omit":n.credentials="same-origin",n}function s(i){if(i.ep)return;i.ep=!0;const n=e(i);fetch(i.href,n)}})();const p=r=>80*Math.pow(400/80,r),u=(r,t,e)=>t*Math.pow(e/t,r);class m{constructor({onStateChange:t=()=>{},onError:e=s=>console.error("[ChatterboxEngine] Error:",s)}={}){this.audioContext=null,this.node=null,this.pitch=.3,this.voiced=!0,this.aspirated=!1,this.noiseLevel=.2,this.formants={f1:.5,f2:.4,f3:.5,f4:.5},this.envelope={attack:.01,release:.15},this.reverb={size:.3,level:.2},this.masterGain=.8,this.onStateChange=t,this.onError=e,this.ready=!1,this.pendingMessages=[]}async initialize(){if(!this.ready)try{const t=new(window.AudioContext||window.webkitAudioContext);this.audioContext=t,await t.audioWorklet.addModule(new URL(""+new URL("chatterbox-worklet-k1Ykzt-a.js",import.meta.url).href,import.meta.url));const e=new AudioWorkletNode(t,"chatterbox-processor",{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2]});e.port.onmessage=s=>this.handleWorkletMessage(s.data),e.onprocessorerror=s=>this.onError(s),e.connect(t.destination),this.node=e,this.ready=!0,this.postStateToWorklet({type:"init",sampleRate:t.sampleRate,pitch:p(this.pitch),voiced:this.voiced,aspirated:this.aspirated,noiseLevel:this.noiseLevel,formants:this.getResolvedFormants(),envelope:this.envelope,reverb:this.reverb,master:this.masterGain}),this.flushPendingMessages(),this.notifyState()}catch(t){this.onError(t)}}async ensureRunning(){this.audioContext||await this.initialize(),this.audioContext.state==="suspended"&&(await this.audioContext.resume(),this.notifyState())}suspend(){this.audioContext&&this.audioContext.state!=="closed"&&this.audioContext.suspend().then(()=>this.notifyState())}setPitch(t){this.pitch=Math.min(Math.max(t,0),1),this.postStateToWorklet({type:"pitch",value:p(this.pitch)})}setVoiced(t){this.voiced=!!t,this.postStateToWorklet({type:"voiced",value:this.voiced})}setAspirated(t){this.aspirated=!!t,this.postStateToWorklet({type:"aspirated",value:this.aspirated})}setNoiseLevel(t){this.noiseLevel=Math.min(Math.max(t,0),1),this.postStateToWorklet({type:"noiseLevel",value:this.noiseLevel})}setFormant(t,e){const s=`f${t+1}`;if(this.formants[s]!==void 0){this.formants[s]=Math.min(Math.max(e,0),1);const i=this.getResolvedFormants();this.postStateToWorklet({type:"formant",index:t,frequency:i[t].frequency,bandwidth:i[t].bandwidth})}}setMasterGain(t){this.masterGain=Math.min(Math.max(t,0),1),this.postStateToWorklet({type:"master",value:this.masterGain})}setEnvelope({attack:t,release:e}){typeof t=="number"&&(this.envelope.attack=t),typeof e=="number"&&(this.envelope.release=e),this.postStateToWorklet({type:"envelope",value:this.envelope})}setReverb({size:t,level:e}){typeof t=="number"&&(this.reverb.size=t),typeof e=="number"&&(this.reverb.level=e),this.postStateToWorklet({type:"reverb",value:this.reverb})}noteOn({midi:t,frequency:e,velocity:s=1}){this.ready&&this.postStateToWorklet({type:"noteOn",midi:t,frequency:e,velocity:s,time:this.audioContext.currentTime})}noteOff({midi:t}){this.ready&&this.postStateToWorklet({type:"noteOff",midi:t,time:this.audioContext.currentTime})}getResolvedFormants(){return[{frequency:u(this.formants.f1,200,1e3),bandwidth:80},{frequency:u(this.formants.f2,500,3e3),bandwidth:120},{frequency:u(this.formants.f3,1500,4e3),bandwidth:150},{frequency:u(this.formants.f4,2500,4500),bandwidth:200}]}handleWorkletMessage(t){t?.type==="log"?console.log("[ChatterboxWorklet]",t.data):t?.type==="error"&&this.onError(new Error(t.message))}postStateToWorklet(t){if(!this.node){this.pendingMessages.push(t);return}this.node.port.postMessage(t)}flushPendingMessages(){!this.node||this.pendingMessages.length===0||(this.pendingMessages.forEach(t=>this.node.port.postMessage(t)),this.pendingMessages.length=0)}notifyState(){this.onStateChange({contextState:this.audioContext?.state??"pending",pitch:this.pitch,voiced:this.voiced,aspirated:this.aspirated,noiseLevel:this.noiseLevel,formants:this.formants,envelope:this.envelope,reverb:this.reverb,masterGain:this.masterGain})}}class g{constructor({canvas:t,width:e=400,height:s=300,onInput:i=(o,l)=>{},onStart:n=()=>{},onEnd:a=()=>{}}){this.canvas=t,this.width=e,this.height=s,this.onInput=i,this.onStart=n,this.onEnd=a,this.x=.5,this.y=.5,this.isDragging=!1,this.vowels=[{label:"i",x:.1,y:.1,color:"#4CAF50"},{label:"e",x:.2,y:.35,color:"#4CAF50"},{label:"a",x:.4,y:.85,color:"#FFC107"},{label:"o",x:.75,y:.4,color:"#FF5722"},{label:"u",x:.85,y:.15,color:"#FF5722"}],this.setupCanvas(),this.setupEventListeners(),this.render()}setupCanvas(){this.canvas.width=this.width,this.canvas.height=this.height,this.ctx=this.canvas.getContext("2d")}setupEventListeners(){this.canvas.addEventListener("mousedown",t=>this.handleStart(t)),window.addEventListener("mousemove",t=>this.handleMove(t)),window.addEventListener("mouseup",()=>this.handleEnd()),this.canvas.addEventListener("touchstart",t=>{t.preventDefault(),this.handleStart(t.touches[0])}),window.addEventListener("touchmove",t=>{this.isDragging&&(t.preventDefault(),this.handleMove(t.touches[0]))}),window.addEventListener("touchend",()=>this.handleEnd()),window.addEventListener("touchcancel",()=>this.handleEnd())}handleStart(t){this.isDragging=!0,this.updatePosition(t),this.onStart()}handleMove(t){this.isDragging&&this.updatePosition(t)}handleEnd(){this.isDragging=!1,this.onEnd()}updatePosition(t){const e=this.canvas.getBoundingClientRect(),s=t.clientX-e.left,i=t.clientY-e.top;this.x=Math.max(0,Math.min(1,s/this.width)),this.y=Math.max(0,Math.min(1,i/this.height)),this.render(),this.onInput(this.x,this.y)}setPosition(t,e){this.x=Math.max(0,Math.min(1,t)),this.y=Math.max(0,Math.min(1,e)),this.render()}render(){const t=this.ctx,e=this.width,s=this.height;t.fillStyle=this.isDragging?"#1a2030":"#1a1a1a",t.fillRect(0,0,e,s),t.strokeStyle="#333",t.lineWidth=1;for(let a=0;a<=4;a++){const o=a/4*e,l=a/4*s;t.beginPath(),t.moveTo(o,0),t.lineTo(o,s),t.stroke(),t.beginPath(),t.moveTo(0,l),t.lineTo(e,l),t.stroke()}t.strokeStyle="#555",t.lineWidth=2,t.beginPath(),t.moveTo(.1*e,.1*s),t.lineTo(.1*e,.85*s),t.lineTo(.4*e,.85*s),t.lineTo(.85*e,.15*s),t.lineTo(.85*e,.1*s),t.closePath(),t.stroke(),t.font="bold 16px sans-serif",t.textAlign="center",t.textBaseline="middle",this.vowels.forEach(a=>{const o=a.x*e,l=a.y*s;t.fillStyle=a.color,t.beginPath(),t.arc(o,l,8,0,Math.PI*2),t.fill(),t.fillStyle="#fff",t.fillText(a.label,o,l)}),t.font="12px sans-serif",t.fillStyle="#888",t.textAlign="left",t.fillText("Front (F2 high)",10,s-10),t.textAlign="right",t.fillText("Back (F2 low)",e-10,s-10),t.textAlign="center",t.fillText("High (F1 low)",e/2,15),t.fillText("Low (F1 high)",e/2,s-5);const i=this.x*e,n=this.y*s;t.strokeStyle="#fff",t.lineWidth=1,t.setLineDash([5,3]),t.beginPath(),t.moveTo(i,0),t.lineTo(i,s),t.moveTo(0,n),t.lineTo(e,n),t.stroke(),t.setLineDash([]),t.fillStyle="rgba(33, 150, 243, 0.7)",t.strokeStyle="#2196F3",t.lineWidth=3,t.beginPath(),t.arc(i,n,12,0,Math.PI*2),t.fill(),t.stroke(),t.fillStyle="#fff",t.beginPath(),t.arc(i,n,4,0,Math.PI*2),t.fill()}destroy(){this.isDragging=!1}}class h{constructor({onNoteOn:t,onNoteOff:e,onControlChange:s=null,onActivity:i=null}={}){this.onNoteOn=typeof t=="function"?t:null,this.onNoteOff=typeof e=="function"?e:null,this.onControlChange=typeof s=="function"?s:null,this.onActivity=typeof i=="function"?i:null,this.midiAccess=null,this.selectedInput=null,this.selectedChannel="all",this.enabled=!0,this.activeNotes=new Map,this.onDeviceChange=null}async initialize(){if(!navigator.requestMIDIAccess)return console.warn("[MidiInputManager] Web MIDI API not available"),!1;try{this.midiAccess=await navigator.requestMIDIAccess(),this.midiAccess.onstatechange=e=>{typeof this.onDeviceChange=="function"&&this.onDeviceChange(this.listInputs()),e.port===this.selectedInput&&e.port.state!=="connected"&&this._disconnectCurrentInput()};const t=this.listInputs();return t.length>0&&this.selectInput(t[0].id),!0}catch(t){return console.error("[MidiInputManager] Failed to initialize",t),!1}}listInputs(){if(!this.midiAccess)return[];const t=[];for(const e of this.midiAccess.inputs.values())t.push({id:e.id,name:e.name||"Unknown Device",manufacturer:e.manufacturer||"",state:e.state});return t}selectInput(t){if(this._disconnectCurrentInput(),!t||!this.midiAccess){this.selectedInput=null;return}const e=this.midiAccess.inputs.get(t);if(!e){console.warn(`[MidiInputManager] Input ${t} not found`);return}this.selectedInput=e,this.selectedInput.onmidimessage=s=>this._handleMessage(s)}_disconnectCurrentInput(){this.selectedInput&&(this.selectedInput.onmidimessage=null)}setChannel(t){if(t==="all"){this.selectedChannel="all";return}const e=parseInt(t,10);Number.isInteger(e)&&e>=1&&e<=16&&(this.selectedChannel=e)}setEnabled(t){this.enabled=!!t,this.enabled||this._releaseAllNotes()}shutdown(){this._releaseAllNotes(),this._disconnectCurrentInput(),this.selectedInput=null}_handleMessage(t){if(!this.enabled)return;const[e,s,i]=t.data,n=e&240,a=(e&15)+1;if(!(this.selectedChannel!=="all"&&a!==this.selectedChannel))switch(this.onActivity&&this.onActivity({messageType:n,channel:a}),n){case 144:i>0?this._handleNoteOn(s,i,a):this._handleNoteOff(s,a);break;case 128:this._handleNoteOff(s,a);break;case 176:this.onControlChange&&this.onControlChange({controller:s,value:i,channel:a});break}}_handleNoteOn(t,e,s){const i=performance.now();if(this.activeNotes.set(t,{velocity:e,channel:s,timestamp:i}),this.onNoteOn){const n={midi:t,velocity:e/127,channel:s,name:h.midiToName(t),frequency:h.midiToFrequency(t)};try{this.onNoteOn(n)}catch(a){console.error("[MidiInputManager] onNoteOn callback failed",a)}}}_handleNoteOff(t,e){if(this.activeNotes.get(t)&&(this.activeNotes.delete(t),this.onNoteOff)){const i={midi:t,channel:e,name:h.midiToName(t)};try{this.onNoteOff(i)}catch(n){console.error("[MidiInputManager] onNoteOff callback failed",n)}}}_releaseAllNotes(){for(const t of this.activeNotes.keys())this._handleNoteOff(t,this.activeNotes.get(t)?.channel??"all");this.activeNotes.clear()}static midiToFrequency(t){return 440*Math.pow(2,(t-69)/12)}static midiToName(t){const e=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"],s=t%12,i=Math.floor(t/12)-1;return`${e[s]}${i}`}}const c=r=>`${Math.round(r)} Hz`,f=r=>r<.01?`${Math.round(r*1e3)}ms`:r<1?`${(r*1e3).toFixed(0)}ms`:`${r.toFixed(2)}s`;class y{constructor(t){this.container=t,this.engineState=null,this.engine=new m({onStateChange:e=>this.handleEngineState(e),onError:e=>this.showError(e)}),this.midi=new h({onNoteOn:e=>this.handleNoteOn(e),onNoteOff:e=>this.handleNoteOff(e),onActivity:()=>this.flashMidiActivity()}),this.joystick=null,this.isJoystickActive=!1,this.currentPitch=120}mount(){this.render(),this.setupPowerButton(),this.setupSourceControls(),this.setupJoystick(),this.setupFormantControls(),this.setupEnvelopeControls(),this.setupMidiControls(),this.setupKeyboardShortcuts(),this.updateStatusPill("pending")}render(){this.container.innerHTML=`
      <div class="app">
        <header class="app__header">
          <h1 class="app__title">Chatterbox Speech Synthesizer</h1>
          <div class="app__status">
            <span class="status-pill status-pill--off" data-status-pill>Power Off</span>
            <button class="power-button" data-power>Power</button>
          </div>
        </header>

        <section class="panel">
          <h2 class="panel__title">Source</h2>
          <div class="source-controls">
            <div class="slider-control">
              <label for="pitch-slider">Pitch</label>
              <input type="range" id="pitch-slider" min="0" max="100" value="30" data-pitch-slider />
              <span data-pitch-value>120 Hz</span>
            </div>
            <div class="slider-control">
              <label for="noise-slider">Noise Level</label>
              <input type="range" id="noise-slider" min="0" max="100" value="20" data-noise-slider />
              <span data-noise-value>20%</span>
            </div>
            <div class="checkbox-group">
              <label>
                <input type="checkbox" data-voiced checked />
                Voiced
              </label>
              <label>
                <input type="checkbox" data-aspirated />
                Aspirated
              </label>
            </div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Vowel Space (F1 & F2)</h2>
          <div class="instructions">
            <p><strong>Click and drag</strong> on the canvas to speak. Hold <kbd>Space</kbd> to trigger sound.</p>
            <p>Move around to change vowel sounds: <strong>i</strong> (top-left), <strong>e</strong>, <strong>a</strong> (bottom), <strong>o</strong>, <strong>u</strong> (top-right)</p>
          </div>
          <div class="joystick-container">
            <canvas data-joystick width="500" height="350"></canvas>
          </div>
          <div class="formant-readout">
            <span>F1: <span data-f1-readout>500 Hz</span></span>
            <span>F2: <span data-f2-readout>1500 Hz</span></span>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Formants F3 & F4</h2>
          <div class="formant-controls">
            <div class="slider-control">
              <label for="f3-slider">F3 Frequency</label>
              <input type="range" id="f3-slider" min="0" max="100" value="50" data-f3-slider />
              <span data-f3-value>2500 Hz</span>
            </div>
            <div class="slider-control">
              <label for="f4-slider">F4 Frequency</label>
              <input type="range" id="f4-slider" min="0" max="100" value="50" data-f4-slider />
              <span data-f4-value>3500 Hz</span>
            </div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Envelope</h2>
          <div class="envelope-controls">
            <div class="slider-control">
              <label for="attack-slider">Attack</label>
              <input type="range" id="attack-slider" min="0" max="100" value="5" data-attack-slider />
              <span data-attack-value>10ms</span>
            </div>
            <div class="slider-control">
              <label for="release-slider">Release</label>
              <input type="range" id="release-slider" min="0" max="100" value="30" data-release-slider />
              <span data-release-value>150ms</span>
            </div>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">Voice Modes (Future Features)</h2>
          <div class="checkbox-group">
            <label>
              <input type="checkbox" disabled />
              Nasal
            </label>
            <label>
              <input type="checkbox" disabled />
              Sing
            </label>
            <label>
              <input type="checkbox" disabled />
              Shout
            </label>
            <label>
              <input type="checkbox" disabled />
              Fry
            </label>
          </div>
        </section>

        <section class="panel">
          <h2 class="panel__title">MIDI</h2>
          <div class="midi-status">
            <span class="status-pill status-pill--off" data-midi-pill>MIDI Offline</span>
            <div class="midi-status__devices">
              <label for="midi-devices">Device</label>
              <select id="midi-devices" data-midi-select disabled>
                <option>No Devices</option>
              </select>
            </div>
            <label>
              <input type="checkbox" class="toggle" data-midi-toggle checked />
            </label>
            <div class="midi-activity" data-midi-activity></div>
          </div>
        </section>
      </div>
    `}setupPowerButton(){this.container.querySelector("[data-power]").addEventListener("click",async()=>{try{!this.engine.audioContext||this.engine.audioContext.state==="suspended"?await this.engine.ensureRunning():this.engine.suspend()}catch(e){this.showError(e)}})}setupSourceControls(){const t=this.container.querySelector("[data-pitch-slider]"),e=this.container.querySelector("[data-pitch-value]");t.addEventListener("input",o=>{const l=o.target.value/100;this.engine.setPitch(l);const d=80*Math.pow(400/80,l);this.currentPitch=d,e.textContent=c(d)});const s=this.container.querySelector("[data-noise-slider]"),i=this.container.querySelector("[data-noise-value]");s.addEventListener("input",o=>{const l=o.target.value/100;this.engine.setNoiseLevel(l),i.textContent=`${Math.round(l*100)}%`}),this.container.querySelector("[data-voiced]").addEventListener("change",o=>{this.engine.setVoiced(o.target.checked)}),this.container.querySelector("[data-aspirated]").addEventListener("change",o=>{this.engine.setAspirated(o.target.checked)})}setupJoystick(){const t=this.container.querySelector("[data-joystick]"),e=this.container.querySelector("[data-f1-readout]"),s=this.container.querySelector("[data-f2-readout]");this.joystick=new g({canvas:t,width:500,height:350,onInput:(i,n)=>{const a=1-i;this.engine.setFormant(1,a);const o=500*Math.pow(3e3/500,a);s.textContent=c(o);const l=n;this.engine.setFormant(0,l);const d=200*Math.pow(1e3/200,l);e.textContent=c(d)},onStart:async()=>{this.isJoystickActive||(this.isJoystickActive=!0,this.container.querySelector("[data-joystick]").classList.add("active"),await this.handleNoteOn({midi:60,frequency:this.currentPitch,velocity:.8}))},onEnd:()=>{this.isJoystickActive&&(this.isJoystickActive=!1,this.container.querySelector("[data-joystick]").classList.remove("active"),this.handleNoteOff({midi:60}))}})}setupFormantControls(){const t=this.container.querySelector("[data-f3-slider]"),e=this.container.querySelector("[data-f3-value]");t.addEventListener("input",n=>{const a=n.target.value/100;this.engine.setFormant(2,a);const o=1500*Math.pow(4e3/1500,a);e.textContent=c(o)});const s=this.container.querySelector("[data-f4-slider]"),i=this.container.querySelector("[data-f4-value]");s.addEventListener("input",n=>{const a=n.target.value/100;this.engine.setFormant(3,a);const o=2500*Math.pow(4500/2500,a);i.textContent=c(o)})}setupEnvelopeControls(){const t=this.container.querySelector("[data-attack-slider]"),e=this.container.querySelector("[data-attack-value]");t.addEventListener("input",n=>{const a=n.target.value/100,o=.001*Math.pow(1/.001,a);this.engine.setEnvelope({attack:a}),e.textContent=f(o)});const s=this.container.querySelector("[data-release-slider]"),i=this.container.querySelector("[data-release-value]");s.addEventListener("input",n=>{const a=n.target.value/100,o=.01*Math.pow(3/.01,a);this.engine.setEnvelope({release:a}),i.textContent=f(o)})}setupMidiControls(){const t=this.container.querySelector("[data-midi-toggle]"),e=this.container.querySelector("[data-midi-select]"),s=this.container.querySelector("[data-midi-pill]");t.addEventListener("change",i=>{const n=i.target.checked;this.midi.setEnabled(n),this.updateMidiStatus(n)}),e.addEventListener("change",i=>{const n=i.target.value;this.midi.selectInput(n)}),this.midi.onDeviceChange=i=>{if(e.innerHTML="",i.length===0){e.innerHTML="<option>No Devices</option>",e.disabled=!0;return}i.forEach(n=>{const a=document.createElement("option");a.value=n.id,a.textContent=n.name,e.appendChild(a)}),e.disabled=!1},this.initMidi().then(i=>{s.textContent=i?"MIDI Ready":"MIDI Unavailable",s.classList.toggle("status-pill--off",!i),this.updateMidiStatus(i&&t.checked)})}async initMidi(){return await this.midi.initialize()}updateMidiStatus(t){const e=this.container.querySelector("[data-midi-pill]");e&&(t?(e.textContent="MIDI Active",e.classList.remove("status-pill--off")):(e.textContent="MIDI Muted",e.classList.add("status-pill--off")))}flashMidiActivity(){const t=this.container.querySelector("[data-midi-activity]");t&&(t.classList.add("midi-activity--on"),clearTimeout(this.midiActivityTimeout),this.midiActivityTimeout=setTimeout(()=>{t.classList.remove("midi-activity--on")},150))}async handleNoteOn(t){try{await this.engine.ensureRunning(),this.engine.noteOn(t)}catch(e){this.showError(e)}}handleNoteOff(t){this.engine.noteOff(t)}handleEngineState(t){this.engineState=t,this.updateStatusPill(t.contextState)}updateStatusPill(t){const e=this.container.querySelector("[data-status-pill]");if(!e)return;let s="Power Off",i=!1;switch(t){case"running":s="Audio Running",i=!1;break;case"suspended":s="Suspended",i=!0;break;case"closed":s="Closed",i=!0;break;default:s="Ready",i=!0}e.textContent=s,e.classList.toggle("status-pill--off",i)}setupKeyboardShortcuts(){window.addEventListener("keydown",async t=>{t.code==="Space"&&!t.repeat&&(t.preventDefault(),this.isJoystickActive||(this.isJoystickActive=!0,this.container.querySelector("[data-joystick]").classList.add("active"),await this.handleNoteOn({midi:60,frequency:this.currentPitch,velocity:.8})))}),window.addEventListener("keyup",t=>{t.code==="Space"&&(t.preventDefault(),this.isJoystickActive&&(this.isJoystickActive=!1,this.container.querySelector("[data-joystick]").classList.remove("active"),this.handleNoteOff({midi:60})))})}showError(t){console.error("[Chatterbox] Error",t),window.dispatchEvent(new CustomEvent("chatterbox:error",{detail:t}))}}const v=()=>{const r=document.getElementById("app");if(!r)throw new Error("[Chatterbox] #app container missing");new y(r).mount()};document.readyState==="loading"?document.addEventListener("DOMContentLoaded",v):v();
//# sourceMappingURL=index-BjweGOaL.js.map
