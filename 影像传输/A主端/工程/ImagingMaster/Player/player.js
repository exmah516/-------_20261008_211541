"use strict";
(() => {
  const states = [];
  let selected = "dsa";
  let timer;
  function stop() {
    clearInterval(timer);
    for (const s of states) {
      const reader = s.reader;
      s.reader = null;
      reader?.close();
      s.receiver = null;
      if (s.callback !== undefined) s.video.cancelVideoFrameCallback(s.callback);
      s.video.srcObject = null;
    }
    states.length = 0;
    document.getElementById("streams").replaceChildren();
  }
  function connect(s) {
    const previousReader = s.reader;
    s.reader = null;
    previousReader?.close();
    s.receiver = null;
    s.video.srcObject = null;
    s.lastDecoded = performance.now();
    s.decodedFrames = 0;
    s.generation++;
    s.error = "";
    s.connectionState = s.iceConnectionState = "";
    s.section.classList.add("stale");
    s.cover.textContent = "连接中…";
    const reader = new MediaMTXWebRTCReader({
      url: s.config.url,
      onError: error => {
        if (s.reader !== reader) return;
        s.error = String(error);
        s.section.classList.add("stale");
        s.cover.textContent = "暂无画面 · 自动重连";
        s.cover.title = s.error;
        s.receiver = null;
      },
      onTrack: event => {
        if (s.reader !== reader) return;
        if (event.track.kind !== "video") return;
        s.error = "";
        s.cover.title = "";
        s.receiver = event.receiver;
        // 浏览器仍需处理包重排；目标为最小延迟，并不承诺物理上的零缓冲。
        try { if ("jitterBufferTarget" in s.receiver) s.receiver.jitterBufferTarget = 0; } catch (_) { }
        try { if ("playoutDelayHint" in s.receiver) s.receiver.playoutDelayHint = 0; } catch (_) { }
        s.lastDecoded = performance.now();
        s.decodedFrames = 0;
        s.previousCount = s.previousDelay = 0;
        s.stats = {};
        s.video.srcObject = new MediaStream([event.track]);
        s.video.play().catch(error => { s.error = String(error); });
      }
    });
    s.reader = reader;
  }
  function frame(s) {
    s.presented++;
    s.section.classList.remove("stale");
    s.callback = s.video.requestVideoFrameCallback(() => frame(s));
  }
  async function sample(s) {
    const receiver = s.receiver;
    if (!receiver || s.sampling) return;
    s.sampling = true;
    try {
      const report = await receiver.getStats();
      if (s.receiver !== receiver) return;
      report.forEach(r => {
        if (r.type !== "inbound-rtp" || r.kind !== "video") return;
        const codec = report.get(r.codecId);
        const deltaCount = (r.jitterBufferEmittedCount || 0) - (s.previousCount || 0);
        const transport = report.get(r.transportId);
        const pair = report.get(transport?.selectedCandidatePairId);
        const localCandidate = report.get(pair?.localCandidateId);
        const remoteCandidate = report.get(pair?.remoteCandidateId);
        const decodedFrames = r.framesDecoded || 0;
        if (decodedFrames > s.decodedFrames) s.lastDecoded = performance.now();
        s.decodedFrames = decodedFrames;
        s.stats = {
          framesDecoded: decodedFrames,
          packetsLost: r.packetsLost || 0,
          nackCount: r.nackCount || 0,
          codec: codec?.mimeType || "",
          protocol: localCandidate?.protocol || remoteCandidate?.protocol || "",
          decoder: r.decoderImplementation || "",
          powerEfficientDecoder: r.powerEfficientDecoder,
          jitterBufferMs: deltaCount > 0 ? 1000 * ((r.jitterBufferDelay || 0) - (s.previousDelay || 0)) / deltaCount : 0
        };
        s.previousCount = r.jitterBufferEmittedCount || 0;
        s.previousDelay = r.jitterBufferDelay || 0;
      });
    } catch (_) { /* 已关闭的连接下一轮会恢复。 */ }
    finally {
      s.sampling = false;
      if (s.receiver === receiver) {
        // Frame presentation callbacks can pause while WebView is hidden; use decoded RTP frames for link health.
        const age = performance.now() - s.lastDecoded;
        if (age > 3000) {
          s.section.classList.add("stale");
          s.cover.textContent = "画面中断 · 等待恢复";
        }
        if (age > 8000) connect(s);
      }
    }
  }
  function select(key) {
    if (!states.some(s => s.config.key === key)) key = "dsa";
    selected = key;
    let side = 0;
    for (const s of states) {
      s.section.classList.remove("main", "side1", "side2");
      s.section.classList.add(s.config.key === key ? "main" : "side" + (++side));
    }
  }
  function snapshot() {
    return { selected, streams: states.map(s => ({
      key: s.config.key, generation: s.generation, presented: s.presented,
      stale: s.section.classList.contains("stale"), width: s.video.videoWidth, height: s.video.videoHeight,
      objectFit: getComputedStyle(s.video).objectFit, error: s.error, ...s.stats
    })) };
  }
  function start(config) {
    stop();
    for (const item of config) {
      const section = document.createElement("section");
      section.className = "stale";
      const video = document.createElement("video");
      video.autoplay = video.muted = video.playsInline = true;
      video.setAttribute("aria-label", item.caption);
      const caption = document.createElement("div"); caption.className = "caption"; caption.textContent = item.caption;
      const cover = document.createElement("div"); cover.className = "cover";
      section.append(video, caption, cover);
      document.getElementById("streams").append(section);
      const s = { config: item, section, video, cover, presented: 0, generation: 0, stats: {}, error: "" };
      states.push(s);
      s.callback = video.requestVideoFrameCallback(() => frame(s));
      connect(s);
    }
    select(selected);
    timer = setInterval(() => {
      for (const s of states) {
        void sample(s);
      }
    }, 1000);
  }
  window.player = { start, stop, select, snapshot };
  window.addEventListener("pagehide", stop);
})();
