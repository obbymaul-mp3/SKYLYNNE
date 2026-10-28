// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 SKYLYNNE contributors
// SKYLYNNE in an AudioWorklet: skylynne.wasm is the instrument itself (tools/web/sk_web.c). Each render quantum asks it for
// 128 frames, which also runs its clock; about 30 times a second the screen (when it changed) and the LEDs go to the page.
class Skylynne extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.serial = -1;
    this.lastPub = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        this.ex = instance.exports;
        this.ex.web_boot();
        if (m.master != null) this.ex.web_master(m.master | 0);
        this.port.postMessage({ type: "ready" });
      } catch (err) {
        this.port.postMessage({ type: "error", message: String(err) });
      }
      return;
    }
    const ex = this.ex;
    if (!ex) return;
    switch (m.type) {
      case "btn": ex.web_btn(m.label >>> 0, m.down ? 1 : 0); break;
      case "enc": ex.web_enc(m.role >>> 0, m.steps | 0); break;
      case "key": ex.web_key(m.n >>> 0, m.down ? 1 : 0); break;
      case "master": ex.web_master(m.q12 >>> 0); break;
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    if (!ex) return true;
    const out = outputs[0], n = out[0].length;
    ex.web_render(n);
    out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
    if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
    if (currentTime - this.lastPub >= 1 / 30) {
      this.lastPub = currentTime;
      const msg = { type: "frame", leds: new Uint8Array(ex.memory.buffer, ex.web_leds(), 41).slice() };
      const transfer = [];
      const s = ex.web_fb_serial();
      if (s !== this.serial) {
        this.serial = s;
        msg.fb = new Uint32Array(ex.memory.buffer, ex.web_fb(), 57600).slice();
        transfer.push(msg.fb.buffer);
      }
      this.port.postMessage(msg, transfer);
    }
    return true;
  }
}
registerProcessor("skylynne", Skylynne);
