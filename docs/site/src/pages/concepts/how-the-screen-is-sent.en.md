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
- **H.264** needs `openh264` at run time (`libopenh264.so.8`, or the Cisco dll next to `xdesk.exe` on Windows); without it, large moves stay JPEG.
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
