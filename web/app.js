/*
 * xdesk web client
 * Protocol v1 (include/xdesk/net/Protocol.hpp): text frames = JSON control {"t": ...}, binary frames = [u8 type][payload]
 * big endian. Served by the host itself (same origin), no external resource (CSP default-src 'self')
*/
"use strict";

(function () {
    // ---------------------------------------------------------------- //
    // Constants

    const PROTOCOL = 1;
    const BIN = {Frame: 0x10, FrameAck: 0x11, Cursor: 0x12, MouseMove: 0x20, MouseButton: 0x21, MouseWheel: 0x22, Key: 0x23, Text: 0x24, TermData: 0x31};
    const TILE = {Jpeg: 1, Solid: 2};
    const STORAGE = "xdesk";
    const UA = navigator.userAgent;
    const IN_APP = /xdesk-app\//.test(UA);
    const KIND = IN_APP ? "apk" : "web";
    const LONG_PRESS = 550; // ms before a still finger becomes a right click
    const TAP_SLOP = 10; // px a tap may move

    // ---------------------------------------------------------------- //
    // Translations

    const I18N = {
        en: {},
        fr: {
            skip: "Aller au contenu", apkText: "Installez l'application xdesk pour Android : plein écran, clavier, mises à jour automatiques.",
            apkInstall: "Installer l'application", dismiss: "Plus tard", connectEyebrow: "Accès à distance",
            connectTitle: "Se connecter à cet ordinateur",
            connectLead: "Entrez le code affiché sur l'écran de l'hôte. La connexion est chiffrée de bout en bout (TLS 1.3, échange de clés post-quantique).",
            codeLabel: "Code de connexion", codeHelp: "Inutile pour un appareil de confiance.", nameLabel: "Nom de cet appareil",
            remember: "Se souvenir de cet appareil (de confiance après accord)", fpLabel: "Empreinte de l'hôte (à comparer avec l'écran de l'hôte)",
            connect: "Se connecter", forget: "Oublier cet appareil", reconnect: "Se reconnecter", back: "Retour", screen: "Écran",
            terminal: "Terminal", newTerminal: "Nouveau terminal", view: "Voir", control: "Contrôler", fit: "Ajuster", actual: "1:1",
            quality: "Qualité", disconnect: "Déconnecter", noView: "L'hôte n'a pas donné le droit de voir l'écran à cette session.",
            waiting: "En attente", pendingTitle: "En attente de l'hôte", pendingText: "La personne devant l'ordinateur doit accepter la connexion.",
            connecting: "Connexion...", closedTitle: "Session terminée", bannedTitle: "Appareil banni", deniedTitle: "Connexion refusée",
            bannedText: "Cet appareil a été banni par l'hôte. Seule une action locale sur l'ordinateur peut le débannir.",
            lostText: "La connexion a été perdue.", codeRequired: "Entrez le code affiché sur l'hôte.",
            controlDenied: "L'hôte n'a pas donné le droit de contrôle.", terminalDenied: "L'hôte n'a pas donné le droit d'ouvrir un terminal.",
            proofMismatch: "L'hôte n'a pas prouvé qu'il connaît le secret de cet appareil : connexion fermée.",
            exited: "processus terminé", copied: "Copié", fullscreenFail: "Plein écran indisponible",
            states: {idle: "hors ligne", connecting: "connexion", pending: "en attente", active: "connecté", closed: "fermé", banned: "banni"},
        },
    };
    const EN_STATES = {idle: "offline", connecting: "connecting", pending: "pending", active: "connected", closed: "closed", banned: "banned"};
    const EN_TEXT = {
        waiting: "Waiting", pendingTitle: "Waiting for the host", pendingText: "The person in front of the computer has to accept the connection.",
        connecting: "Connecting...", closedTitle: "Session ended", bannedTitle: "Device banned", deniedTitle: "Connection refused",
        bannedText: "This device was banned by the host. Only a local action on the computer can unban it.",
        lostText: "The connection was lost.", codeRequired: "Enter the code shown on the host.",
        controlDenied: "The host did not give the control right.", terminalDenied: "The host did not give the terminal right.",
        proofMismatch: "The host did not prove it knows the secret of this device: connection closed.",
        exited: "process exited", copied: "Copied", fullscreenFail: "Fullscreen unavailable",
    };
    let lang = load("lang") || ((navigator.language || "en").toLowerCase().startsWith("fr") ? "fr" : "en");

    function t(key) {
        const table = I18N[lang] || {};
        if (key in table) return table[key];
        if (key in EN_TEXT) return EN_TEXT[key];
        return key;
    }

    function applyLanguage() {
        document.documentElement.lang = lang;
        $("langBtn").textContent = lang.toUpperCase();
        for (const el of document.querySelectorAll("[data-i18n]")) {
            if (!el.dataset.i18nEn) el.dataset.i18nEn = el.textContent;
            const tr = (I18N[lang] || {})[el.dataset.i18n];
            el.textContent = tr !== undefined ? tr : el.dataset.i18nEn;
        }
        for (const el of document.querySelectorAll("[data-i18n-title]")) {
            const tr = (I18N[lang] || {})[el.dataset.i18nTitle];
            if (!el.dataset.i18nTitleEn) el.dataset.i18nTitleEn = el.title;
            el.title = tr !== undefined ? tr : el.dataset.i18nTitleEn;
        }
        setStatus(state.status);
    }

    // ---------------------------------------------------------------- //
    // Tools

    function $(id) {return document.getElementById(id);}

    function load(name) {
        try {return localStorage.getItem(STORAGE + "-" + name);} catch (e) {return null;}
    }

    function save(name, value) {
        try {
            if (value === null || value === undefined) localStorage.removeItem(STORAGE + "-" + name);
            else localStorage.setItem(STORAGE + "-" + name, value);
        } catch (e) { /* private mode: nothing kept */ }
    }

    function b64encode(bytes) {
        let s = "";
        for (let i = 0; i < bytes.length; ++i) s += String.fromCharCode(bytes[i]);
        return btoa(s);
    }

    function b64decode(text) {
        const s = atob(text || "");
        const out = new Uint8Array(s.length);
        for (let i = 0; i < s.length; ++i) out[i] = s.charCodeAt(i);
        return out;
    }

    function hexdecode(text) {
        const clean = (text || "").replace(/[^0-9a-fA-F]/g, "");
        const out = new Uint8Array(clean.length >> 1);
        for (let i = 0; i < out.length; ++i) out[i] = parseInt(clean.substr(i * 2, 2), 16);
        return out;
    }

    function hexencode(bytes) {
        return Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
    }

    function concat(...parts) {
        let size = 0;
        for (const p of parts) size += p.length;
        const out = new Uint8Array(size);
        let pos = 0;
        for (const p of parts) {out.set(p, pos); pos += p.length;}
        return out;
    }

    function randomBytes(size) {
        const out = new Uint8Array(size);
        crypto.getRandomValues(out);
        return out;
    }

    function equalBytes(a, b) {
        if (a.length !== b.length) return false;
        let diff = 0;
        for (let i = 0; i < a.length; ++i) diff |= a[i] ^ b[i];
        return diff === 0;
    }

    // Code normalization of the host: upper case, separators removed, O -> 0, I / L -> 1
    function normalizeCode(code) {
        return (code || "").toUpperCase().replace(/[\s-]/g, "").replace(/O/g, "0").replace(/[IL]/g, "1");
    }

    function formatCode(code) {
        const clean = (code || "").toUpperCase().replace(/[^0-9A-Z]/g, "").slice(0, 12);
        return clean.replace(/(.{3})(?=.)/g, "$1-");
    }

    function guessName() {
        let os = "Device";
        if (/Android/i.test(UA)) os = /Mobile/i.test(UA) ? "Phone - Android" : "Tablet - Android";
        else if (/iPhone/i.test(UA)) os = "Phone - iOS";
        else if (/iPad/i.test(UA)) os = "Tablet - iPadOS";
        else if (/Windows/i.test(UA)) os = "Windows";
        else if (/Mac OS X/i.test(UA)) os = "macOS";
        else if (/CrOS/i.test(UA)) os = "ChromeOS";
        else if (/Linux/i.test(UA)) os = "Linux";
        let browser = "browser";
        if (/Firefox\//.test(UA)) browser = "Firefox";
        else if (/Edg\//.test(UA)) browser = "Edge";
        else if (/Chrome\//.test(UA)) browser = "Chrome";
        else if (/Safari\//.test(UA)) browser = "Safari";
        return os + (IN_APP ? " (apk)" : " - " + browser);
    }

    function guessOs() {
        const m = UA.match(/Android [\d.]+|Windows NT [\d.]+|Mac OS X [\d_]+|iPhone OS [\d_]+|CrOS|Linux/);
        return m ? m[0].replace(/_/g, ".") : "unknown";
    }

    let toastTimer = 0;
    function toast(text) {
        const el = $("toast");
        el.textContent = text;
        el.hidden = false;
        clearTimeout(toastTimer);
        toastTimer = setTimeout(() => {el.hidden = true;}, 3200);
    }

    // ---------------------------------------------------------------- //
    // State

    const state = {
        status: "idle",
        info: null, // /api/info
        ws: null,
        serverNonce: null,
        clientNonce: null,
        method: "",
        secret: null, // device secret used for this auth
        perms: {view: false, control: false, terminal: false, clipboard: false},
        mode: "view",
        frameW: 0,
        frameH: 0,
        drawQueue: Promise.resolve(),
        frames: 0,
        bytes: 0,
        latency: -1,
        closedByUs: false,
        gotBye: false,
        terms: new Map(), // id -> {term, fit, el, tab, exited}
        activeTab: "screen",
        nextTerm: 1,
        pressed: new Set(), // key codes down (released on blur)
        buttons: new Set(), // mouse buttons down
    };

    function setStatus(status) {
        state.status = status;
        const el = $("status");
        el.dataset.state = status;
        const table = (I18N[lang] || {}).states || EN_STATES;
        el.textContent = table[status] || status;
    }

    function showView(name) {
        for (const v of ["connectView", "waitView", "sessionView"]) $(v).hidden = v !== name;
    }

    function getDevice() {
        try {
            const d = JSON.parse(load("device") || "null");
            if (d && d.id && d.secret) return d;
        } catch (e) { /* corrupted: ignored */ }
        return null;
    }

    // ---------------------------------------------------------------- //
    // Theme

    function applyTheme(theme) {
        if (theme === "light" || theme === "dark") document.documentElement.dataset.theme = theme;
        else delete document.documentElement.dataset.theme;
    }

    $("themeBtn").addEventListener("click", () => {
        const dark = document.documentElement.dataset.theme
            ? document.documentElement.dataset.theme === "dark"
            : matchMedia("(prefers-color-scheme: dark)").matches;
        const next = dark ? "light" : "dark";
        applyTheme(next);
        save("theme", next);
    });

    $("langBtn").addEventListener("click", () => {
        lang = lang === "fr" ? "en" : "fr";
        save("lang", lang);
        applyLanguage();
    });

    // ---------------------------------------------------------------- //
    // Host info, apk banner

    async function fetchInfo() {
        try {
            const res = await fetch("/api/info", {cache: "no-store"});
            if (!res.ok) return;
            state.info = await res.json();
            $("hostName").textContent = state.info.name || "";
            if (state.info.name) document.title = "xdesk - " + state.info.name;
            if (state.info.fingerprint) {
                $("fpText").textContent = state.info.fingerprint;
                $("fpBox").hidden = false;
            }
            const android = /Android/i.test(UA);
            const dismissed = load("apk-dismissed") === (state.info.apk || "");
            if (android && !IN_APP && state.info.apk && !dismissed) {
                $("apkLink").textContent = $("apkLink").textContent.replace(/ \(.*\)$/, "") + " (" + state.info.apk + ")";
                $("apkBanner").hidden = false;
            }
        } catch (e) { /* offline: the websocket error tells it */ }
    }

    $("apkClose").addEventListener("click", () => {
        $("apkBanner").hidden = true;
        save("apk-dismissed", (state.info && state.info.apk) || "");
    });

    // ---------------------------------------------------------------- //
    // Auth

    /*
     * transcript = UTF-8("xdesk-auth-v1|" + role) + serverNonce (32) + clientNonce (32) + fingerprint (32)
     * The browser cannot read the TLS certificate: the fingerprint part is the one announced by /api/info (hex of
     * "AB:CD:..."). For a web client it does not bind the channel (the native clients use the real certificate); the
     * device secret itself is only sent once, inside TLS, in the welcome of the first approved auth
    */
    function transcript(role) {
        const fp = hexdecode(state.info && state.info.fingerprint);
        return concat(new TextEncoder().encode("xdesk-auth-v1|" + role), state.serverNonce, state.clientNonce, fp);
    }

    async function hmac(secret, data) {
        const key = await crypto.subtle.importKey("raw", secret, {name: "HMAC", hash: "SHA-256"}, false, ["sign"]);
        return new Uint8Array(await crypto.subtle.sign("HMAC", key, data));
    }

    async function onChallenge(msg) {
        state.serverNonce = b64decode(msg.nonce);
        if (msg.host) $("hostName").textContent = msg.host;
        const device = getDevice();
        const code = normalizeCode($("codeInput").value);
        const remember = $("rememberInput").checked;
        if (msg.known && device && crypto.subtle) {
            state.method = "device";
            state.secret = hexdecode(device.secret);
            const proof = await hmac(state.secret, transcript("client"));
            sendJson({t: "auth", method: "device", proof: hexencode(proof), remember: true});
            return;
        }
        if (!code) {
            closeSocket();
            showConnect(t("codeRequired"));
            $("codeInput").focus();
            return;
        }
        state.method = "code";
        state.secret = null;
        sendJson({t: "auth", method: "code", code: code, remember: remember});
    }

    async function onWelcome(msg) {
        if (state.method === "device" && msg.proof && state.secret && crypto.subtle) {
            const expected = await hmac(state.secret, transcript("server"));
            if (!equalBytes(expected, hexdecode(msg.proof))) {
                closeSocket();
                showClosed(t("deniedTitle"), t("proofMismatch"), false);
                return;
            }
        }
        if (msg.device && msg.device.id && msg.device.secret) {
            save("device", JSON.stringify({id: msg.device.id, secret: msg.device.secret}));
            $("forgetBtn").hidden = false;
        }
        $("codeInput").value = "";
        state.perms = Object.assign({view: false, control: false, terminal: false, clipboard: false}, msg.perms || {});
        setStatus("active");
        showView("sessionView");
        applyPerms();
        setMode(state.perms.control ? "control" : "view", true);
        sendQuality();
        $("screen").focus();
    }

    // ---------------------------------------------------------------- //
    // Connection

    function connect() {
        closeSocket();
        state.closedByUs = false;
        state.gotBye = false;
        state.clientNonce = randomBytes(32);
        setStatus("connecting");
        showWait(t("connecting"), "", true);
        const name = ($("nameInput").value || "").trim() || guessName();
        save("name", name);

        const ws = new WebSocket("wss://" + location.host + "/ws");
        ws.binaryType = "arraybuffer";
        state.ws = ws;
        ws.onopen = () => {
            const device = getDevice();
            sendJson({t: "hello", v: PROTOCOL, kind: KIND, name: name, os: guessOs(), device: device ? device.id : "", nonce: b64encode(state.clientNonce)});
        };
        ws.onmessage = (e) => {
            if (typeof e.data === "string") onText(e.data);
            else onBinary(new Uint8Array(e.data));
        };
        ws.onclose = () => {
            if (state.ws !== ws) return;
            state.ws = null;
            releaseAll();
            if (state.closedByUs || state.gotBye) return;
            if (state.status === "active" || state.status === "pending" || state.status === "connecting") showClosed(t("closedTitle"), t("lostText"), false);
        };
    }

    function closeSocket() {
        if (!state.ws) return;
        state.closedByUs = true;
        try {state.ws.close(1000);} catch (e) { /* already closed */ }
        state.ws = null;
    }

    function sendJson(obj) {
        if (state.ws && state.ws.readyState === WebSocket.OPEN) state.ws.send(JSON.stringify(obj));
    }

    function sendBin(bytes) {
        if (state.ws && state.ws.readyState === WebSocket.OPEN) state.ws.send(bytes);
    }

    function onText(text) {
        let msg;
        try {msg = JSON.parse(text);} catch (e) {return;}
        switch (msg.t) {
            case "challenge": onChallenge(msg); break;
            case "pending": setStatus("pending"); showWait(t("pendingTitle"), t("pendingText"), true); break;
            case "welcome": onWelcome(msg); break;
            case "denied": state.gotBye = true; closeSocket(); showClosed(t("deniedTitle"), msg.reason || "", false); break;
            case "bye": state.gotBye = true; closeSocket(); showClosed(msg.banned ? t("bannedTitle") : t("closedTitle"), msg.banned ? (msg.reason ? msg.reason + " - " : "") + t("bannedText") : (msg.reason || ""), !!msg.banned); break;
            case "ping": sendJson({t: "pong", ts: msg.ts}); break;
            case "pong": state.latency = Math.max(0, Math.round(performance.now() - msg.ts)); break;
            case "perms": state.perms = Object.assign(state.perms, msg.perms || {}); applyPerms(); break;
            case "screen": /* the next frame carries the new size */ break;
            case "info": toast(msg.text || ""); break;
            case "term_exit": onTermExit(msg.id, msg.code); break;
            case "clip": if (msg.text && navigator.clipboard && document.hasFocus()) navigator.clipboard.writeText(msg.text).catch(() => {}); break;
            default: break;
        }
    }

    function onBinary(data) {
        state.bytes += data.length;
        if (data.length < 1) return;
        switch (data[0]) {
            case BIN.Frame: onFrame(data); break;
            case BIN.Cursor: onCursor(data); break;
            case BIN.TermData: onTermData(data); break;
            default: break;
        }
    }

    // ---------------------------------------------------------------- //
    // Screens

    function showConnect(error) {
        setStatus("idle");
        showView("connectView");
        const box = $("connectError");
        box.hidden = !error;
        box.textContent = error || "";
    }

    function showWait(title, text, spinning) {
        showView("waitView");
        $("waitTitle").textContent = title;
        $("waitText").textContent = text;
        $("waitSpinner").hidden = !spinning;
        $("reconnectBtn").hidden = true;
        $("waitView").querySelector(".card").classList.remove("banned");
    }

    function showClosed(title, text, banned) {
        releaseAll();
        destroyTerms();
        if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
        setStatus(banned ? "banned" : "closed");
        showView("waitView");
        $("waitTitle").textContent = title;
        $("waitText").textContent = text;
        $("waitSpinner").hidden = true;
        $("reconnectBtn").hidden = banned;
        $("waitView").querySelector(".card").classList.toggle("banned", banned);
    }

    $("reconnectBtn").addEventListener("click", connect);
    $("backBtn").addEventListener("click", () => {closeSocket(); showConnect("");});
    $("disconnectBtn").addEventListener("click", () => {
        closeSocket();
        showClosed(t("closedTitle"), "", false);
    });
    $("forgetBtn").addEventListener("click", () => {
        save("device", null);
        $("forgetBtn").hidden = true;
    });

    $("codeInput").addEventListener("input", (e) => {
        const el = e.target;
        const atEnd = el.selectionStart === el.value.length;
        el.value = formatCode(el.value);
        if (atEnd) el.setSelectionRange(el.value.length, el.value.length);
    });

    $("connectForm").addEventListener("submit", (e) => {
        e.preventDefault();
        if (!getDevice() && !normalizeCode($("codeInput").value)) {
            showConnect(t("codeRequired"));
            return;
        }
        connect();
    });

    // ---------------------------------------------------------------- //
    // Frames

    const canvas = $("screen");
    const ctx = canvas.getContext("2d", {alpha: false, desynchronized: true});
    const cursorEl = $("cursor");
    let cursorPos = {x: -1, y: -1, visible: false};

    function onFrame(data) {
        const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
        if (data.length < 11) return;
        const id = view.getUint32(1);
        const w = view.getUint16(5);
        const h = view.getUint16(7);
        const count = view.getUint16(9);
        const tiles = [];
        let pos = 11;
        for (let i = 0; i < count; ++i) {
            if (pos + 9 > data.length) return;
            const tile = {x: view.getUint16(pos), y: view.getUint16(pos + 2), w: view.getUint16(pos + 4), h: view.getUint16(pos + 6), enc: data[pos + 8]};
            pos += 9;
            if (tile.enc === TILE.Jpeg) {
                if (pos + 4 > data.length) return;
                const len = view.getUint32(pos);
                pos += 4;
                if (pos + len > data.length) return;
                tile.blob = new Blob([data.subarray(pos, pos + len)], {type: "image/jpeg"});
                pos += len;
            } else if (tile.enc === TILE.Solid) {
                if (pos + 3 > data.length) return;
                tile.color = "rgb(" + data[pos] + "," + data[pos + 1] + "," + data[pos + 2] + ")";
                pos += 3;
            } else {
                return; // unknown encoding: the frame cannot be trusted
            }
            tiles.push(tile);
        }
        // Decode in parallel, draw in order (a frame never overtakes the previous one)
        const decoded = Promise.all(tiles.map((tl) => tl.blob ? createImageBitmap(tl.blob).catch(() => null) : null));
        state.drawQueue = state.drawQueue.then(() => decoded).then((bitmaps) => {
            if (w !== state.frameW || h !== state.frameH) {
                state.frameW = w;
                state.frameH = h;
                canvas.width = w;
                canvas.height = h;
                layoutCanvas();
            }
            for (let i = 0; i < tiles.length; ++i) {
                const tl = tiles[i];
                if (tl.color) {
                    ctx.fillStyle = tl.color;
                    ctx.fillRect(tl.x, tl.y, tl.w, tl.h);
                } else if (bitmaps[i]) {
                    ctx.drawImage(bitmaps[i], tl.x, tl.y, tl.w, tl.h);
                    bitmaps[i].close();
                }
            }
            const ack = new Uint8Array(5);
            ack[0] = BIN.FrameAck;
            new DataView(ack.buffer).setUint32(1, id);
            sendBin(ack);
            state.frames += 1;
            placeCursor();
        }).catch(() => {});
    }

    function onCursor(data) {
        if (data.length < 6) return;
        const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
        cursorPos = {x: view.getInt16(1), y: view.getInt16(3), visible: data[5] !== 0};
        placeCursor();
    }

    // Box where the frame is really drawn inside the canvas element (object-fit: contain letterboxes it)
    function drawnBox() {
        const r = canvas.getBoundingClientRect();
        if (!r.width || !r.height || !state.frameW || !state.frameH) return null;
        const ratio = Math.min(r.width / state.frameW, r.height / state.frameH);
        const w = state.frameW * ratio;
        const h = state.frameH * ratio;
        return {left: r.left + (r.width - w) / 2, top: r.top + (r.height - h) / 2, ratio: ratio};
    }

    function placeCursor() {
        // Own cursor while controlling with a mouse: the remote one would only lag behind it
        const hide = !cursorPos.visible || cursorPos.x < 0 || !state.frameW || (state.mode === "control" && !touchMode);
        const box = hide ? null : drawnBox();
        cursorEl.hidden = !box;
        if (!box) return;
        const stage = $("stage").getBoundingClientRect();
        cursorEl.style.transform = "translate(" + (box.left - stage.left + cursorPos.x * box.ratio) + "px," + (box.top - stage.top + cursorPos.y * box.ratio) + "px)";
    }

    // ---------------------------------------------------------------- //
    // Layout, zoom, fullscreen

    function layoutCanvas() {
        const fit = $("fitSelect").value === "fit";
        $("viewport").classList.toggle("fit", fit);
        if (fit) {
            canvas.style.width = "";
            canvas.style.height = "";
        } else {
            const ratio = window.devicePixelRatio || 1;
            canvas.style.width = (state.frameW / ratio) + "px";
            canvas.style.height = (state.frameH / ratio) + "px";
        }
        requestAnimationFrame(placeCursor);
    }

    $("fitSelect").addEventListener("change", () => {save("fit", $("fitSelect").value); layoutCanvas();});
    window.addEventListener("resize", () => {layoutCanvas(); fitActiveTerm();});

    $("fullBtn").addEventListener("click", () => {
        const el = $("sessionView");
        if (document.fullscreenElement) document.exitFullscreen().catch(() => {});
        else if (el.requestFullscreen) el.requestFullscreen({navigationUI: "hide"}).catch(() => toast(t("fullscreenFail")));
        else toast(t("fullscreenFail"));
    });
    document.addEventListener("fullscreenchange", () => {layoutCanvas(); fitActiveTerm();});

    // ---------------------------------------------------------------- //
    // Rights, mode, quality

    function applyPerms() {
        $("modeControl").disabled = !state.perms.control;
        $("cadBtn").disabled = !state.perms.control;
        $("kbdBtn").disabled = !state.perms.control;
        $("newTermBtn").hidden = !state.perms.terminal;
        $("noView").hidden = !!state.perms.view;
        $("viewport").hidden = !state.perms.view;
        if (!state.perms.control && state.mode === "control") setMode("view", false);
        if (!state.perms.terminal) destroyTerms();
    }

    function setMode(mode, send) {
        if (mode === "control" && !state.perms.control) {
            toast(t("controlDenied"));
            return;
        }
        if (mode !== "control") releaseAll();
        state.mode = mode;
        $("modeView").classList.toggle("on", mode === "view");
        $("modeControl").classList.toggle("on", mode === "control");
        $("screenPane").classList.toggle("control", mode === "control");
        if (send) sendJson({t: "mode", mode: mode});
        placeCursor();
    }

    $("modeView").addEventListener("click", () => setMode("view", true));
    $("modeControl").addEventListener("click", () => setMode("control", true));

    function sendQuality() {
        const quality = parseInt($("qualityInput").value, 10);
        const fps = parseInt($("fpsSelect").value, 10);
        save("quality", String(quality));
        save("fps", String(fps));
        sendJson({t: "quality", quality: quality, scale: 1.0, fps: fps});
    }

    $("qualityInput").addEventListener("change", sendQuality);
    $("fpsSelect").addEventListener("change", sendQuality);

    // Stats + ping every second
    setInterval(() => {
        if (state.status === "active") sendJson({t: "ping", ts: performance.now()});
        $("statLatency").textContent = (state.latency >= 0 ? state.latency : "--") + " ms";
        $("statFps").textContent = state.frames + " fps";
        $("statKbps").textContent = Math.round(state.bytes * 8 / 1000) + " kb/s";
        state.frames = 0;
        state.bytes = 0;
    }, 1000);

    // ---------------------------------------------------------------- //
    // Input encoding

    function canControl() {
        return state.status === "active" && state.mode === "control" && state.perms.control && state.activeTab === "screen";
    }

    function toFrame(clientX, clientY) {
        const box = drawnBox();
        if (!box) return null;
        const x = Math.round((clientX - box.left) / box.ratio);
        const y = Math.round((clientY - box.top) / box.ratio);
        return {x: Math.max(0, Math.min(state.frameW - 1, x)), y: Math.max(0, Math.min(state.frameH - 1, y))};
    }

    function sendMove(p) {
        const m = new Uint8Array(5);
        const v = new DataView(m.buffer);
        m[0] = BIN.MouseMove;
        v.setUint16(1, p.x);
        v.setUint16(3, p.y);
        sendBin(m);
    }

    function sendButton(button, down) {
        if (down) state.buttons.add(button);
        else state.buttons.delete(button);
        sendBin(new Uint8Array([BIN.MouseButton, button, down ? 1 : 0]));
    }

    function sendWheel(dx, dy) {
        const m = new Uint8Array(5);
        const v = new DataView(m.buffer);
        m[0] = BIN.MouseWheel;
        v.setInt16(1, Math.max(-32768, Math.min(32767, Math.round(dx))));
        v.setInt16(3, Math.max(-32768, Math.min(32767, Math.round(dy))));
        sendBin(m);
    }

    function sendKey(code, down) {
        if (!code || code.length > 64) return;
        if (down) state.pressed.add(code);
        else state.pressed.delete(code);
        const bytes = new TextEncoder().encode(code);
        sendBin(concat(new Uint8Array([BIN.Key, down ? 1 : 0, bytes.length]), bytes));
    }

    function sendText(text) {
        if (!text) return;
        sendBin(concat(new Uint8Array([BIN.Text]), new TextEncoder().encode(text)));
    }

    function releaseAll() {
        for (const code of Array.from(state.pressed)) sendKey(code, false);
        for (const b of Array.from(state.buttons)) sendButton(b, false);
    }

    // DOM button -> protocol button (0 left, 1 middle, 2 right, 3 back, 4 forward): the same numbers
    function mapButton(b) {return b >= 0 && b <= 4 ? b : -1;}

    // ---------------------------------------------------------------- //
    // Mouse (pointer events)

    let touchMode = false;
    const touches = new Map(); // pointerId -> {x, y, startX, startY, t}
    let touchState = {longTimer: 0, long: false, dragging: false, scrollY: 0, scrollX: 0, twoFinger: false};

    canvas.addEventListener("contextmenu", (e) => e.preventDefault());

    canvas.addEventListener("pointerdown", (e) => {
        canvas.focus({preventScroll: true});
        if (e.pointerType === "touch") {onTouchDown(e); return;}
        touchMode = false;
        if (!canControl()) return;
        e.preventDefault();
        canvas.setPointerCapture(e.pointerId);
        const p = toFrame(e.clientX, e.clientY);
        const b = mapButton(e.button);
        if (!p || b < 0) return;
        sendMove(p);
        sendButton(b, true);
    });

    canvas.addEventListener("pointermove", (e) => {
        if (e.pointerType === "touch") {onTouchMove(e); return;}
        if (!canControl()) return;
        const events = e.getCoalescedEvents ? e.getCoalescedEvents() : [];
        const last = events.length ? events[events.length - 1] : e;
        const p = toFrame(last.clientX, last.clientY);
        if (p) sendMove(p);
    });

    canvas.addEventListener("pointerup", (e) => {
        if (e.pointerType === "touch") {onTouchUp(e); return;}
        if (!canControl()) return;
        e.preventDefault();
        const b = mapButton(e.button);
        if (b >= 0) sendButton(b, false);
    });

    canvas.addEventListener("pointercancel", (e) => {
        if (e.pointerType === "touch") {touches.delete(e.pointerId); clearTimeout(touchState.longTimer);}
        releaseAll();
    });

    canvas.addEventListener("wheel", (e) => {
        if (!canControl()) return;
        e.preventDefault();
        // 120 per notch, positive = up (-deltaY); pixels: ~100 px per notch, lines: 3 lines per notch
        const unit = e.deltaMode === 1 ? 40 : (e.deltaMode === 2 ? 1200 : 1.2);
        sendWheel(-e.deltaX * unit, -e.deltaY * unit);
    }, {passive: false});

    // ---------------------------------------------------------------- //
    // Touch: tap = click, one finger drag = left drag, long press = right click, two fingers = scroll

    function onTouchDown(e) {
        touchMode = true;
        if (!canControl()) return;
        e.preventDefault();
        canvas.setPointerCapture(e.pointerId);
        touches.set(e.pointerId, {x: e.clientX, y: e.clientY, startX: e.clientX, startY: e.clientY, t: performance.now()});
        if (touches.size === 1) {
            touchState = {longTimer: 0, long: false, dragging: false, scrollY: 0, scrollX: 0, twoFinger: false};
            const p = toFrame(e.clientX, e.clientY);
            if (p) sendMove(p);
            touchState.longTimer = setTimeout(() => {
                touchState.long = true;
                sendButton(2, true);
                sendButton(2, false);
                if (navigator.vibrate) navigator.vibrate(15);
            }, LONG_PRESS);
        } else {
            clearTimeout(touchState.longTimer);
            touchState.twoFinger = true;
            if (touchState.dragging) {sendButton(0, false); touchState.dragging = false;}
        }
    }

    function onTouchMove(e) {
        const tp = touches.get(e.pointerId);
        if (!tp || !canControl()) return;
        e.preventDefault();
        const dx = e.clientX - tp.x;
        const dy = e.clientY - tp.y;
        tp.x = e.clientX;
        tp.y = e.clientY;
        if (touchState.twoFinger) {
            if (touches.size >= 2) {
                // Average of the fingers: each finger reports half the move
                touchState.scrollY += dy / touches.size;
                touchState.scrollX += dx / touches.size;
                const sy = Math.trunc(touchState.scrollY / 4);
                const sx = Math.trunc(touchState.scrollX / 4);
                if (sy || sx) {
                    sendWheel(sx * 12, sy * 12);
                    touchState.scrollY -= sy * 4;
                    touchState.scrollX -= sx * 4;
                }
            }
            return;
        }
        const moved = Math.hypot(e.clientX - tp.startX, e.clientY - tp.startY) > TAP_SLOP;
        if (moved && !touchState.long) {
            clearTimeout(touchState.longTimer);
            if (!touchState.dragging) {
                touchState.dragging = true;
                const start = toFrame(tp.startX, tp.startY);
                if (start) sendMove(start);
                sendButton(0, true);
            }
        }
        if (touchState.dragging) {
            const p = toFrame(e.clientX, e.clientY);
            if (p) sendMove(p);
        }
    }

    function onTouchUp(e) {
        const tp = touches.get(e.pointerId);
        touches.delete(e.pointerId);
        clearTimeout(touchState.longTimer);
        if (!tp || !canControl()) return;
        e.preventDefault();
        if (touches.size > 0) return;
        if (touchState.dragging) {
            sendButton(0, false);
        } else if (!touchState.long && !touchState.twoFinger && performance.now() - tp.t < 600) {
            const p = toFrame(tp.startX, tp.startY);
            if (p) sendMove(p);
            sendButton(0, true);
            sendButton(0, false);
        }
        touchState.dragging = false;
    }

    // ---------------------------------------------------------------- //
    // Keyboard

    const kbdInput = $("kbdInput");

    window.addEventListener("keydown", (e) => {
        if (!canControl() || e.target === kbdInput) return;
        if (document.activeElement && document.activeElement !== canvas && document.activeElement !== document.body && document.activeElement.closest(".toolbar")) return;
        if (!e.code) return; // soft keyboards: handled by the hidden input
        e.preventDefault();
        sendKey(e.code, true);
    });

    window.addEventListener("keyup", (e) => {
        if (!canControl() || e.target === kbdInput) return;
        if (!e.code || !state.pressed.has(e.code)) return;
        e.preventDefault();
        sendKey(e.code, false);
    });

    window.addEventListener("blur", releaseAll);
    document.addEventListener("visibilitychange", () => {if (document.hidden) releaseAll();});

    $("cadBtn").addEventListener("click", () => {
        if (!canControl()) {toast(t("controlDenied")); return;}
        sendKey("ControlLeft", true);
        sendKey("AltLeft", true);
        sendKey("Delete", true);
        sendKey("Delete", false);
        sendKey("AltLeft", false);
        sendKey("ControlLeft", false);
    });

    // On-screen keyboard: a hidden input opens the soft keyboard, its text is sent as Text messages
    $("kbdBtn").addEventListener("click", () => {
        if (!state.perms.control) {toast(t("controlDenied")); return;}
        if (state.mode !== "control") setMode("control", true);
        kbdInput.value = "";
        kbdInput.focus();
    });

    kbdInput.addEventListener("beforeinput", (e) => {
        if (!canControl()) return;
        switch (e.inputType) {
            case "insertText": case "insertReplacementText": case "insertCompositionText": break;
            case "insertLineBreak": case "insertParagraph": e.preventDefault(); sendKey("Enter", true); sendKey("Enter", false); break;
            case "deleteContentBackward": e.preventDefault(); sendKey("Backspace", true); sendKey("Backspace", false); break;
            case "deleteContentForward": e.preventDefault(); sendKey("Delete", true); sendKey("Delete", false); break;
            default: break;
        }
    });

    kbdInput.addEventListener("input", (e) => {
        if (!canControl()) return;
        if (e.isComposing) return;
        if (kbdInput.value) sendText(kbdInput.value);
        kbdInput.value = "";
    });

    kbdInput.addEventListener("compositionend", () => {
        if (kbdInput.value) sendText(kbdInput.value);
        kbdInput.value = "";
    });

    kbdInput.addEventListener("keydown", (e) => {
        // Hardware keys pressed while the hidden input has the focus (Enter, arrows, Escape...)
        const special = ["Enter", "Escape", "Tab", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Home", "End", "PageUp", "PageDown"];
        if (!canControl() || !special.includes(e.key)) return;
        e.preventDefault();
        const code = e.code || e.key;
        sendKey(code, true);
        sendKey(code, false);
    });

    // ---------------------------------------------------------------- //
    // Terminals (xterm.js)

    function selectTab(name) {
        state.activeTab = name;
        for (const b of document.querySelectorAll("#tabs .tab[data-tab]")) b.classList.toggle("active", b.dataset.tab === name);
        $("screenPane").hidden = name !== "screen";
        $("termPane").hidden = name === "screen";
        $("screenTools").hidden = name !== "screen";
        if (name !== "screen") releaseAll();
        for (const [id, tm] of state.terms) tm.el.hidden = "term" + id !== name;
        if (name !== "screen") {
            const tm = state.terms.get(parseInt(name.slice(4), 10));
            if (tm) {fitTerm(tm); tm.term.focus();}
        } else {
            canvas.focus({preventScroll: true});
        }
    }

    $("tabs").addEventListener("click", (e) => {
        const close = e.target.closest(".close");
        if (close) {
            e.stopPropagation();
            closeTerm(parseInt(close.dataset.id, 10), true);
            return;
        }
        const tab = e.target.closest(".tab[data-tab]");
        if (tab) selectTab(tab.dataset.tab);
    });

    $("newTermBtn").addEventListener("click", openTerm);

    function openTerm() {
        if (!state.perms.terminal) {toast(t("terminalDenied")); return;}
        if (typeof Terminal === "undefined") return;
        let id = state.nextTerm;
        for (let i = 0; i < 255 && state.terms.has(id); ++i) id = id % 255 + 1;
        state.nextTerm = id % 255 + 1;

        const el = document.createElement("div");
        el.className = "term";
        $("termPane").appendChild(el);
        const term = new Terminal({
            cursorBlink: true,
            fontFamily: "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace",
            fontSize: window.innerWidth < 600 ? 12 : 14,
            scrollback: 5000,
            allowProposedApi: false,
            theme: {background: "#101114", foreground: "#e6e7ea", cursor: "#7d9cf0", selectionBackground: "#2f5bd366"},
        });
        const fit = new FitAddon.FitAddon();
        term.loadAddon(fit);
        term.open(el);

        const tab = document.createElement("button");
        tab.type = "button";
        tab.className = "tab";
        tab.dataset.tab = "term" + id;
        tab.textContent = t("terminal") + " " + id + " ";
        const close = document.createElement("span");
        close.className = "close";
        close.dataset.id = String(id);
        close.setAttribute("role", "button");
        close.setAttribute("aria-label", "Close");
        close.textContent = "×";
        tab.appendChild(close);
        $("tabs").insertBefore(tab, $("newTermBtn"));

        const tm = {id: id, term: term, fit: fit, el: el, tab: tab, exited: false, cols: 0, rows: 0};
        state.terms.set(id, tm);
        const encoder = new TextEncoder();
        term.onData((d) => {
            if (tm.exited) return;
            sendBin(concat(new Uint8Array([BIN.TermData, id]), encoder.encode(d)));
        });
        term.onBinary((d) => {
            if (tm.exited) return;
            const bytes = new Uint8Array(d.length);
            for (let i = 0; i < d.length; ++i) bytes[i] = d.charCodeAt(i) & 0xff;
            sendBin(concat(new Uint8Array([BIN.TermData, id]), bytes));
        });
        selectTab("term" + id);
        fitTerm(tm);
        sendJson({t: "term_open", id: id, cols: tm.cols || 80, rows: tm.rows || 24});
        new ResizeObserver(() => fitTerm(tm)).observe(el);
    }

    function fitTerm(tm) {
        if (tm.el.hidden || !tm.el.clientWidth) return;
        try {tm.fit.fit();} catch (e) {return;}
        if (tm.term.cols !== tm.cols || tm.term.rows !== tm.rows) {
            const first = tm.cols === 0;
            tm.cols = tm.term.cols;
            tm.rows = tm.term.rows;
            if (!first && !tm.exited) sendJson({t: "term_resize", id: tm.id, cols: tm.cols, rows: tm.rows});
        }
    }

    function fitActiveTerm() {
        if (state.activeTab === "screen") return;
        const tm = state.terms.get(parseInt(state.activeTab.slice(4), 10));
        if (tm) fitTerm(tm);
    }

    function onTermData(data) {
        if (data.length < 2) return;
        const tm = state.terms.get(data[1]);
        if (tm) tm.term.write(data.subarray(2));
    }

    function onTermExit(id, code) {
        const tm = state.terms.get(id);
        if (!tm) return;
        tm.exited = true;
        tm.tab.classList.add("exited");
        tm.term.write("\r\n\x1b[2m[" + t("exited") + (code !== undefined ? " (" + code + ")" : "") + "]\x1b[0m\r\n");
    }

    function closeTerm(id, send) {
        const tm = state.terms.get(id);
        if (!tm) return;
        if (send && !tm.exited) sendJson({t: "term_close", id: id});
        tm.term.dispose();
        tm.el.remove();
        tm.tab.remove();
        state.terms.delete(id);
        if (state.activeTab === "term" + id) selectTab("screen");
    }

    function destroyTerms() {
        for (const id of Array.from(state.terms.keys())) closeTerm(id, false);
        if (state.activeTab !== "screen") selectTab("screen");
    }

    // ---------------------------------------------------------------- //
    // Start

    applyTheme(load("theme"));
    applyLanguage();
    $("nameInput").value = load("name") || guessName();
    $("qualityInput").value = load("quality") || "70";
    $("fpsSelect").value = load("fps") || "30";
    $("fitSelect").value = load("fit") || "fit";
    $("forgetBtn").hidden = !getDevice();
    const params = new URLSearchParams(location.search);
    if (params.get("code")) $("codeInput").value = formatCode(params.get("code"));
    layoutCanvas();
    showConnect("");
    fetchInfo().then(() => {
        // A trusted device (or a code in the link) connects right away
        if (getDevice() || params.get("code")) connect();
    });
})();
