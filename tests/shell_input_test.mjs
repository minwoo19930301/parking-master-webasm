import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import test from 'node:test';
import assert from 'node:assert/strict';

// Execute the shipped shell's real handlers without a browser or physical input.
function shell() {
  const html = readFileSync(new URL('../web/shell.html', import.meta.url), 'utf8');
  const elements = new Map();
  const document = new EventTarget();
  document.hidden = false;
  class Element extends EventTarget {
    constructor(id, tag = '') {
      super(); this.id = id; this.style = {}; this.dataset = {};
      this.hidden = /\bhidden\b/.test(tag); this.disabled = /\bdisabled\b/.test(tag);
      const classes = new Set();
      this.classList = { add: x => classes.add(x), remove: x => classes.delete(x), toggle: (x,v) => v ? classes.add(x) : classes.delete(x), contains: x => classes.has(x) };
      for (const m of tag.matchAll(/data-([\w-]+)="([^"]*)"/g)) this.dataset[m[1]] = m[2];
    }
    setAttribute() {}
    setPointerCapture() {}
    releasePointerCapture() {}
    getContext() { return {}; }
    getBoundingClientRect() { return {left:0,top:0,width:200,height:200}; }
    focus() { document.activeElement = this; }
    appendChild() {}
    prepend() {}
    closest() { return this; }
    querySelectorAll() { return []; }
    click() { this.dispatchEvent(new Event('click')); }
  }
  for (const tag of html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)) elements.set(tag[1], new Element(tag[1], tag[0]));
  document.body = new Element('body');
  document.getElementById = id => elements.get(id);
  document.querySelector = selector => new Element(selector);
  document.querySelectorAll = selector => [...elements.values()].filter(element => selector.includes('data-latch') ? 'latch' in element.dataset : selector.includes('data-hold') ? 'hold' in element.dataset : false);
  const window = new EventTarget();
  Object.assign(window, {innerWidth:1280,innerHeight:720,setTimeout:()=>0});
  vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1], {
    window, document, screen: {orientation:{angle:0}}, requestAnimationFrame:()=>1, cancelAnimationFrame:()=>{}, console,
  });
  const fire = (id, type, props = {}) => elements.get(id).dispatchEvent(Object.assign(new Event(type, {cancelable:true}), props));
  return { window, document, elements, fire };
}

test('menu pauses driving; resume preserves the session and clears queued input', () => {
  const {window, elements, fire} = shell();
  assert.equal(window.__examIsPaused(), false); // Native app boots directly into free drive.
  fire('open-menu', 'click');
  assert.equal(elements.get('resume-drive-button').hidden, false);
  assert.equal(window.__examIsPaused(), true);
  fire('free-drive-button', 'pointerdown');
  assert.equal(window.__examIsPaused(), false);
  assert.equal(window.__examInput.freeDrivePressed, true);
  window.__examInput.throttle = true;
  window.__examInput.gearReversePressed = true;
  fire('open-menu', 'click');
  assert.equal(window.__examIsPaused(), true);
  assert.equal(window.__examInput.throttle, false);
  assert.equal(window.__examInput.gearReversePressed, false);
  assert.equal(elements.get('resume-drive-button').hidden, false);
  fire('resume-drive-button', 'click');
  assert.equal(window.__examIsPaused(), false);
  assert.equal(window.__examInput.freeDrivePressed, false);
  assert.equal(window.__examInput.startPressed, false);
  assert.equal(window.__examResetClock, true);
});

test('all four road buttons start independently and clear stale input on switching',()=>{
  const {window,elements,fire}=shell();
  for(const [id,key] of [['road-a','roadAPressed'],['road-b','roadBPressed'],['road-c','roadCPressed'],['road-d','roadDPressed']]){
    fire('open-menu','click');window.__examInput.cameraPressed=true;
    fire(id,'pointerdown');assert.equal(window.__examInput[key],true);
    assert.equal(window.__examInput.cameraPressed,false);
    assert.equal(window.__examIsPaused(),false);assert.equal(elements.get('intro-modal').hidden,true);
    assert.equal(window.__examInput.startPressed,false);
  }
});

test('camera toggle uses one-shot input and keyboard respects focus and pause', () => {
  const {window,document,elements,fire}=shell();
  const key=(target,repeat=false)=>{
    const event=Object.assign(new Event('keydown',{cancelable:true}),{code:'KeyT',repeat});
    Object.defineProperty(event,'target',{value:target});
    window.dispatchEvent(event);
  };
  key(elements.get('camera-toggle'));
  assert.equal(window.__examInput.cameraPressed,false);
  key(elements.get('canvas'),true);
  assert.equal(window.__examInput.cameraPressed,false);
  key(elements.get('canvas'));
  assert.equal(window.__examInput.cameraPressed,true);
  assert.equal(window.__examInput.startPressed,false);
  assert.equal(window.__examInput.freeDrivePressed,false);
  fire('open-menu','click');
  key(document.body);
  assert.equal(window.__examInput.cameraPressed,false);
  fire('resume-drive-button','click');
  fire('camera-toggle','pointerdown');
  assert.equal(window.__examInput.cameraPressed,true);
  window.__examCameraState(true);
  assert.equal(elements.get('camera-toggle').textContent,'시점: 3인칭 · T');
  window.__examCameraState(false);
  assert.equal(elements.get('camera-toggle').textContent,'시점: 운전석 · T');
});

test('tutorial releases held controls and keeps the menu paused after dismissal', () => {
  const {window, fire} = shell();
  fire('open-menu','click');
  window.__examInput.throttle = true;
  window.__examInput.steerValue = 1;
  fire('tutorial-open','click');
  assert.equal(window.__examInput.throttle, false);
  assert.equal(window.__examInput.steerValue, 0);
  fire('free-drive-button','pointerdown');
  assert.equal(window.__examInput.freeDrivePressed, false);
  assert.equal(window.__examIsPaused(), true);
  fire('tutorial-skip','click');
  assert.equal(window.__examIsPaused(), true);
  fire('start-button','pointerdown');
  assert.equal(window.__examIsPaused(), false);
  assert.equal(window.__examInput.startPressed, true);
});

test('tab hiding releases pedals and marks the next simulation clock for reset', () => {
  const {window, document, fire} = shell();
  fire('free-drive-button','pointerdown');
  window.__examInput.throttle = true;
  window.__examResetClock = false;
  document.hidden = true;
  document.dispatchEvent(new Event('visibilitychange'));
  assert.equal(window.__examInput.throttle, false);
  assert.equal(window.__examIsPaused(), true);
  assert.equal(window.__examResetClock, true);
  document.hidden = false;
  document.dispatchEvent(new Event('visibilitychange'));
  assert.equal(window.__examIsPaused(), false);
  assert.equal(window.__examResetClock, true);
});

test('Enter starts an exam only from the driving surface, not DOM controls', () => {
  const {window, document, elements, fire} = shell();
  const enter = (target, repeat = false) => {
    const event = Object.assign(new Event('keydown'), {key:'Enter', repeat});
    Object.defineProperty(event, 'target', {value:target});
    window.dispatchEvent(event);
  };
  fire('open-menu', 'click');
  enter(elements.get('resume-drive-button'));
  fire('resume-drive-button', 'click');
  assert.equal(window.__examInput.startPressed, false);
  for (const id of ['control-seatbelt', 'control-throttle', 'resume-drive-button']) {
    enter(elements.get(id));
    assert.equal(window.__examInput.startPressed, false);
  }
  enter(elements.get('canvas'), true);
  assert.equal(window.__examInput.startPressed, false);
  enter(elements.get('canvas'));
  assert.equal(window.__examInput.startPressed, true);
  fire('open-menu', 'click');
  enter(document.body);
  assert.equal(window.__examInput.startPressed, false);
  fire('resume-drive-button', 'click');
  enter(document.body);
  assert.equal(window.__examInput.startPressed, true);
});

test('keyboard pedals keep independent keys, respect dialogs, and release on blur', () => {
  const {window, document, elements, fire} = shell();
  const key = (type, code, target = document.body) => {
    const event = Object.assign(new Event(type, {cancelable:true}), {code});
    Object.defineProperty(event, 'target', {value:target});
    window.dispatchEvent(event);
  };
  key('keydown','KeyW');
  key('keydown','ArrowUp');
  assert.equal(window.__examPedalInput('throttle'),true);
  assert.equal(elements.get('keyboard-throttle').dataset.held,'true');
  key('keyup','KeyW');
  assert.equal(window.__examPedalInput('throttle'),true);
  key('keydown','Space');
  assert.equal(window.__examPedalInput('brake'),true);
  window.dispatchEvent(new Event('blur'));
  assert.equal(window.__examPedalInput('throttle'),false);
  assert.equal(window.__examPedalInput('brake'),false);
  fire('open-menu','click');
  key('keydown','KeyW');
  assert.equal(window.__examPedalInput('throttle'),false);
  fire('resume-drive-button','click');
  key('keydown','Space',{tagName:'BUTTON'});
  key('keydown','KeyW',{tagName:'INPUT'});
  assert.equal(window.__examPedalInput('brake'),false);
  assert.equal(window.__examPedalInput('throttle'),false);
  key('keydown','KeyS');
  assert.equal(window.__examPedalInput('brake'),true);
  key('keyup','KeyS');
  assert.equal(window.__examPedalInput('brake'),false);
});

test('keyboard control activation never bubbles into a pedal; wheel focus still permits pedals', () => {
  const {window,elements}=shell();
  const gear=elements.get('gear-gate');
  gear.dataset.gear='D';
  const event=Object.assign(new Event('keydown',{cancelable:true}),{key:' ',code:'Space'});
  Object.defineProperty(event,'target',{value:gear});
  gear.dispatchEvent(event);
  assert.equal(event.defaultPrevented,true);
  assert.equal(window.__examInput.gearDrivePressed,true);
  window.dispatchEvent(event);
  assert.equal(window.__examPedalInput('brake'),false);
  const unhandled=Object.assign(new Event('keydown',{cancelable:true}),{key:'w',code:'KeyW'});
  Object.defineProperty(unhandled,'target',{value:gear});
  window.dispatchEvent(unhandled);
  assert.equal(window.__examPedalInput('throttle'),false);
  const driving=Object.assign(new Event('keydown',{cancelable:true}),{key:'w',code:'KeyW'});
  Object.defineProperty(driving,'target',{value:elements.get('steer-zone')});
  window.dispatchEvent(driving);
  assert.equal(window.__examPedalInput('throttle'),true);
});

test('steering hit region follows the rendered wheel bounds', () => {
  const {window,elements}=shell();
  window.__examWheelBounds(400,700,160,1440,900);
  const style=elements.get('steer-zone').style;
  assert.equal(parseFloat(style.left),240/1440*100);
  assert.equal(parseFloat(style.top),540/900*100);
  assert.equal(parseFloat(style.width),320/1440*100);
  assert.equal(parseFloat(style.height),320/900*100);
});
