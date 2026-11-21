+++
title = "How the screen is sent"
summary = "Why a still screen costs nothing, how scrolling and video are handled, and how the stream adapts to a slow or lossy link."
section = "concepts"
order = 2
kind = "explain"
level = "simple"
keywords = "screen stream tiles 64x64 dirty jpeg solid palette copy scroll h264 openh264 motion frames in flight ack ping bandwidth udp aes-gcm parity nack lanes websockets"
+++

:::simple tldr
**In one sentence:** xdesk only sends the parts of the screen that changed, reuses what the viewer already has when you scroll, and sends less or blurrier pictures while the link is slow, then sharp ones again.
:::

:::technical tldr
**Definition:** a per-session dirty-tile encoder (64×64 tiles hashed, JPEG / solid / palette / copy tiles, H.264 for large moves on a bottleneck link) driven by an ack-based flow control (frames in flight sized from the measured ping and bandwidth), over TLS websockets, encrypted UDP for the native viewers, or up to 4 websockets for the web viewers on lossy paths.
:::

## The idea {#idea}

:::simple
Think of a whiteboard filmed for someone far away. If only one word changes, you don't send a new photo of the whole board: you send the corner with the word. If the text slides up, you say "move this part up" and only send the new line at the bottom.
:::

:::technical
The host keeps, per session, the hash of every tile the viewer has. A capture frame only produces tiles whose hash changed; a capture with nothing new produces nothing. The encoding of a tile is chosen from its content (solid, palette, JPEG) and from the history (copy of a previous region), and the pace from the acks of the viewer.
:::

## How it works {#how}

:::html
<figure class="stepper" data-steps="6">
  <ol class="captions">
    <li><b>First frame.</b> The viewer has nothing yet: every 64×64 tile is sent.</li>
    <li><b>Hash.</b> Something moves: the tiles are hashed and 3 of the 32 differ from what the viewer has.</li>
    <li><b>Encode.</b> Only those 3 go: a one-color tile as 3 bytes, the changed tiles of one row merged in one JPEG (or a lossless palette for text).</li>
    <li><b>Scroll.</b> The page scrolls by one row: the moved tiles are found higher in the previous frame and sent as <i>copy</i> tiles; only the revealed row is encoded.</li>
    <li><b>Motion.</b> More than a quarter of the tiles change (a video): a lighter JPEG, or one H.264 frame of the whole screen while the link of the viewer is the bottleneck.</li>
    <li><b>Still.</b> The screen stops moving: the same tiles are sent again, sharp.</li>
  </ol>
  <svg viewBox="0 0 760 210" role="img" aria-label="screen tiles sent at each step">
    <rect id="en-tl-panel" class="group" x="350" y="35" width="390" height="150" rx="8"/>
    <rect id="en-tl-t00" class="box" x="30" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t10" class="box" x="68" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t20" class="box" x="106" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t30" class="box" x="144" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t40" class="box" x="182" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t50" class="box" x="220" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t60" class="box" x="258" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t70" class="box" x="296" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t01" class="box" x="30" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t11" class="box" x="68" y="78" width="34" height="34" rx="3" data-hl="0,2,5" data-warn="4,1"/>
    <rect id="en-tl-t21" class="box" x="106" y="78" width="34" height="34" rx="3" data-hl="0,2,5" data-warn="4,1"/>
    <rect id="en-tl-t31" class="box" x="144" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t41" class="box" x="182" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t51" class="box" x="220" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t61" class="box" x="258" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t71" class="box" x="296" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t02" class="box" x="30" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t12" class="box" x="68" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t22" class="box" x="106" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t32" class="box" x="144" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t42" class="box" x="182" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t52" class="box" x="220" y="116" width="34" height="34" rx="3" data-hl="0,2,5" data-warn="4,1"/>
    <rect id="en-tl-t62" class="box" x="258" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t72" class="box" x="296" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="en-tl-t03" class="box" x="30" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t13" class="box" x="68" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t23" class="box" x="106" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t33" class="box" x="144" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t43" class="box" x="182" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t53" class="box" x="220" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t63" class="box" x="258" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-t73" class="box" x="296" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="en-tl-copy" class="group" x="27" y="37" width="306" height="116" rx="6" data-show="3"/>
    <text class="lbl" x="182" y="102" text-anchor="middle" data-show="3">copy</text>
    <text class="lbl" x="370" y="70" data-show="0">32 tiles sent</text>
    <text class="small" x="370" y="96" data-show="0">the whole screen</text>
    <text class="lbl" x="370" y="70" data-show="1">3 tiles changed</text>
    <text class="small" x="370" y="96" data-show="1">29 unchanged: nothing sent</text>
    <text class="lbl" x="370" y="70" data-show="2">3 tiles sent</text>
    <text class="small" x="370" y="96" data-show="2">1 solid (3 bytes), 2 in one JPEG</text>
    <text class="lbl" x="370" y="70" data-show="3">24 copy tiles + 8 encoded</text>
    <text class="small" x="370" y="96" data-show="3">copy = u16 sx, sy (4 bytes)</text>
    <text class="lbl" x="370" y="70" data-show="4">lighter JPEG / H.264</text>
    <text class="small" x="370" y="96" data-show="4">quality lowered while it moves</text>
    <text class="lbl" x="370" y="70" data-show="5">sharp tiles again</text>
    <text class="small" x="370" y="96" data-show="5">quality back to the setting</text>
  </svg>
</figure>
:::

## How fast {#flow}

:::simple
xdesk does not send faster than the viewer can receive: it waits for the viewer to say "got it" before sending more. On a LAN 2 pictures can be on the way, for a far viewer up to 8. When a queue builds up, the quality goes down until the queue is gone.
:::

:::technical
The capture thread ticks every 4 ms. A session gets a frame when its FPS cap allows it and its window is open: without a queue, the frames its rate needs during one ping (2 on a LAN, up to 8 far away); with a queue, only what the path holds (bandwidth × ping) and a lower JPEG quality until it drains, so no lag builds up. A viewer whose link can't follow even at the lowest quality gets a lower resolution (`autoResolution`), raised again once the link is calm. No frame is sent while the viewer shows a terminal.
:::

## Like Chrome Remote Desktop {#crd}

:::simple
Chrome Remote Desktop keeps a sharp picture with almost no lag with a few simple rules, and xdesk follows them. What stays still is sent again sharper as soon as the link has room. What moves is sized for the link so it never queues. Your mouse pointer is drawn on your side, so it never lags. And when you zoom, only the part you see is sent, in more detail.
:::

:::technical
Read in the sources of Chromium (`remoting/`): its encoder wrapper keeps re-encoding the last frame while nothing changes (*top-off*) until the quantizer reaches its floor (10: near lossless), VP9 / AV1 in profile 1 (4:4:4, *lossless color*), only the changed 16 × 16 blocks encoded (*active map*), CBR at the bandwidth WebRTC estimates with no frame dropped by the encoder, a big update encoded at lower quality when it would take longer than a frame interval to send, one encode at a time (a newer frame replaces the waiting one), the cursor shape sent as a bitmap when it changes and drawn by the client at its own mouse. In xdesk: a tile still for 150 ms goes back to the stream quality, after 500 ms up to 90-95 in 4:4:4, within a budget of half the measured bandwidth of a frame interval and only without a queue; only the changed 64 × 64 tiles are encoded; the GPU video runs in CBR at 80 % of the measured bandwidth once the link is the bottleneck (2 frames of buffer); an update larger than two frame intervals of the link is sent lighter at once; the latest capture only, frames in flight bounded by the ping; the portal cursor as metadata, its shape (`CursorShape`) drawn by the viewer at its own mouse while controlling. A zoomed viewer tells the part it shows (`view`): only those tiles are sent, at the resolution its display shows.
:::

## Video on the GPU and the bench {#gpu}

:::simple
When something big moves (a video, a dragged window) and the link of the viewer is the bottleneck, the screen goes as a video. The graphics card makes it (NVENC, Quick Sync, VA-API): 4K at about 60 frames per second with almost no CPU. `xdesk bench` measures every way on your computer, and the host then picks, for each viewer, what gives it the most frames on its link.
:::

:::technical
The encoders of libavcodec (FFmpeg 8, loaded at run time) are probed once: each one opens on a small frame and must give a key frame at once (no encoder that holds frames). A viewer lists the codecs it decodes (`h264`, `hevc`, `av1` in `enc`); the host uses a GPU encoder of the best codec both share (AV1, HEVC, then H.264, or the `videoCodec` setting), openh264 without a GPU (up to 1920 × 1200). Constant quantizer from the quality of the flow control, no B frame, an IDR only when asked, BT.709 limited range; H.264 is constrained baseline like openh264. With a `bench.json` in the data folder, while the link of a viewer is the bottleneck, the encoder of the bench giving the most frames on its measured bandwidth and ping is used (looked at every 2 s, chosen again when the bandwidth moves by 30 %). The resolution stays native as long as possible: the quality goes down first, then the frame rate, then the resolution, never under 1080p unless the viewer asked less.
:::

Measured by `xdesk bench` on a laptop (i7 Alder Lake, RTX 3050 Mobile, Intel iGPU, Optimized build), quality 70, synthetic scenes:

| 4K (3840 × 2160) | encode | per frame | PSNR |
|---|---|---|---|
| video, tiles (JPEG) | 9.0 ms | 379 KB | 36.9 dB |
| video, `hevc_nvenc` | 18 ms | 38 KB | 33.2 dB |
| video, `hevc_vaapi` | 29 ms | 22 KB | 35.7 dB |
| scroll, tiles + copy | 12.6 ms | 10 KB | 32.4 dB |
| dragged window, `hevc_nvenc` | 16.5 ms | 13.6 KB | 34.6 dB |

| Network (modelled) | 4K video | 4K drag | 4K scroll |
|---|---|---|---|
| LAN, 1 Gb/s | tiles, 60 fps | tiles, 60 fps | tiles + copy, 60 fps |
| 4G, 25 Mb/s, 60 ms, 1 % | `hevc_nvenc`, 55 fps | `hevc_nvenc`, 60 fps | tiles + copy, 60 fps |
| 6 Mb/s, 120 ms, 3 % | `hevc_vaapi`, 28 fps | `hevc_nvenc`, 46 fps | tiles + copy, 60 fps |
| 2 Mb/s, 250 ms, 5 % | `hevc_vaapi`, 9 fps | `hevc_vaapi`, 27 fps | tiles + copy, 20 fps |

The terminal output is compressed with zlib for the viewers off the LAN: 3.6× on a build log, worth it under about 440 Mb/s (zstd: 3.7×, lz4: 2.4×, but no decoder in the browsers); the tiles are JPEG already and do not compress (1.05×).

## Over UDP and extra sockets {#transports}

The Qt viewer, the terminal client and the Android app receive the video over **UDP** on the port of the server: AES-256-GCM with one key per direction sent inside TLS, pieces of 1150 bytes, one xor parity per 8 pieces while losses are seen, nacks, and frames that complete in any order. Control, inputs and terminals stay on TLS; the video goes back to TLS when UDP is blocked.

| Link (emulated, 25 s) | TCP + TLS | UDP + GCM + parity |
|---|---|---|
| LAN cable | 30 fps, p95 4.9 ms | 30 fps, p95 13 ms (Python prototype) |
| Fiber, 40 ms | 30 fps, p99 69 ms | 30 fps, p99 50 ms |
| 4G, 80 ms, 1 % loss | 27 fps, p95 163 ms | 27 fps, p95 160 ms |
| 160 ms, 3 % loss, 6 Mbit/s | 17 fps, 15 stalls | 23 fps, 4 stalls |
| 300 ms, 8 % loss, 2 Mbit/s | 5.4 fps, p50 506 ms | 7.8 fps, p50 243 ms |

Browsers have no raw UDP: on a far lossy path a web viewer may get **up to 4 websockets** for the video, each with its own TCP window, kept only when they help. Goodput in Mbit/s:

| Path (ping, loss, link) | 1 socket | 2 | 3 | 4 |
|---|---|---|---|---|
| LAN, 2 ms, 300 Mbit/s | 286 | 290 | 293 | 292 |
| 80 ms, 1 %, 30 Mbit/s | 2.5 | 5.8 | 7.1 | 10.1 |
| 160 ms, 3 %, 6 Mbit/s | 0.9 | 1.4 | 2.0 | 3.2 |
| 300 ms, 8 %, 2 Mbit/s | 0.4 | 0.6 | 1.1 | 1.3 |

[The byte layout of each message](page:protocol#udp).

## Pitfalls {#pitfalls}

- **Wayland** without the PipeWire library falls back to portal screenshots: about 1 frame per second.
- **Video** needs a GPU encoder of the FFmpeg 8 libraries (`libavcodec.so.62`, another major is refused) or `openh264` (`libopenh264.so.8`, or the Cisco dll next to `xdesk.exe` on Windows, up to 1920 × 1200); without both, large moves stay JPEG. HEVC in a browser needs a hardware decoder there; AV1 encoding needs a recent GPU (RTX 40, Arc, Radeon 7000).
- **Extra sockets** cannot lower the ping: they only give back the throughput a lossy path takes from one connection.

## Check yourself {#quiz}

:::html
<details class="quiz"><summary>Nothing moves on the host screen. How much is sent?</summary><p>Nothing: a capture with no new frame is skipped, and unchanged tiles are never sent.</p></details>
<details class="quiz"><summary>Why are copy tiles only used while frames keep their order?</summary><p>A copy reads the frame as it was before the message: if an older frame arrived after a newer one, the copied pixels would be wrong.</p></details>
:::

## Glossary {#glossary}

| Term | Meaning |
|---|---|
| Tile | a 64×64 square of the screen, the unit that is hashed and sent |
| Ack | the viewer telling the host a frame arrived |
| Frames in flight | frames sent but not acked yet |
| Parity | an xor of 8 pieces that rebuilds one lost piece without asking again |
| Lane | an extra websocket that only carries video frames |

Sources: `src/xdesk/platform/FrameEncoder.cpp`, `src/xdesk/host/Stream.cpp`, `include/xdesk/net/Datagram.hpp`; the measures come from netem emulated links.
