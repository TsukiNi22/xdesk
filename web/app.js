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
    const TAP_TIME = 450; // ms a tap may last
    const DOUBLE_TAP = 350; // ms between two taps of a double tap (trackpad: tap then slide = drag)
    const DOUBLE_SLOP = 32; // px between two taps of a double tap (direct: the second one clicks on the first point)
    const MAX_ZOOM = 6; // pinch zoom of the screen
    const TILE_SIZE = 64; // XDESK_TILE_SIZE: frames may complete out of order, each tile keeps the id of the frame that drew it
    const LANE_MAX = 4; // sockets of the video at most (the main one included)
    const LANE_RETIRE = 3000; // ms a socket no longer wanted keeps its frames in flight

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
            screenLoading: "Chargement de l'écran...", controlLoading: "Contrôles en préparation...", controlReady: "Contrôles prêts",
            controlNone: "Contrôle indisponible sur l'hôte", controlPaused: "Contrôle en pause : le panneau xdesk est utilisé sur l'ordinateur (il reprend quand il n'a plus le focus)", controlViewOnly: "Lecture seule : l'hôte n'a pas donné le contrôle",
            controlDenied: "L'hôte n'a pas donné le droit de contrôle.", terminalDenied: "L'hôte n'a pas donné le droit d'ouvrir un terminal.",
            proofMismatch: "L'hôte n'a pas prouvé qu'il connaît le secret de cet appareil : connexion fermée.",
            exited: "processus terminé", copied: "Copié", fullscreenFail: "Plein écran indisponible",
            scan: "Scanner le QR code de l'hôte", scanTitle: "Visez le QR code avec la caméra", scanHint: "Affiché par le panneau de l'hôte : Vue d'ensemble, QR code",
            scanClose: "Annuler", scanNoCamera: "Caméra indisponible : autorisez-la, ou entrez le code à la main.",
            scanNotXdesk: "Ce QR code n'est pas un lien xdesk.", scanOther: "Ce QR code mène à un autre ordinateur : ",
            keysTitle: "Touches spéciales", menu: "Menu", keyboard: "Clavier", paste: "Coller", trackpad: "Pavé", direct: "Direct",
            helpTrackpad: "Pavé tactile : glissez pour bouger le pointeur, touchez pour cliquer, touchez deux fois = double clic (maintenu au second : sélection par mots), touchez puis glissez ou maintenez puis glissez = déplacer, deux doigts = clic droit, glisser à deux doigts = défiler, pincer = zoomer, trois doigts = clavier",
            helpDirect: "Toucher direct : touchez là où cliquer, touchez deux fois = double clic (maintenu au second : sélection par mots), glisser = déplacer, maintenir puis lâcher = clic droit, glisser à deux doigts = défiler, pincer = zoomer, trois doigts = clavier",
            pastePrompt: "Texte à coller sur l'ordinateur distant",
            states: {idle: "hors ligne", connecting: "connexion", pending: "en attente", active: "connecté", closed: "fermé", banned: "banni"},
        },
    };
    const EN_STATES = {idle: "offline", connecting: "connecting", pending: "pending", active: "connected", closed: "closed", banned: "banned"};
    const EN_TEXT = {
        waiting: "Waiting", pendingTitle: "Waiting for the host", pendingText: "The person in front of the computer has to accept the connection.",
        connecting: "Connecting...", closedTitle: "Session ended", bannedTitle: "Device banned", deniedTitle: "Connection refused",
        bannedText: "This device was banned by the host. Only a local action on the computer can unban it.",
        lostText: "The connection was lost.", codeRequired: "Enter the code shown on the host.",
        screenLoading: "Loading the screen...", controlLoading: "Getting the controls ready...", controlReady: "Controls ready",
        controlNone: "Remote control unavailable on the host", controlPaused: "Control paused: the xdesk panel is in use on the computer (back when it loses the focus)", controlViewOnly: "View only: the host did not give the control",
        controlDenied: "The host did not give the control right.", terminalDenied: "The host did not give the terminal right.",
        proofMismatch: "The host did not prove it knows the secret of this device: connection closed.",
        exited: "process exited", copied: "Copied", fullscreenFail: "Fullscreen unavailable",
        scanNoCamera: "Camera unavailable: allow it, or type the code.", scanNotXdesk: "This QR code is not an xdesk link.",
        scanOther: "This QR code leads to another computer: ",
        keyboard: "Keyboard", paste: "Paste", trackpad: "Trackpad", direct: "Direct",
        helpTrackpad: "Trackpad: slide to move the pointer, tap to click, double tap = double click (held on the second: select by words), tap then slide or hold then slide = drag, two fingers tap = right click, two fingers slide = scroll, pinch = zoom, three fingers tap = keyboard",
        helpDirect: "Direct touch: tap where to click, double tap = double click (held on the second: select by words), slide = drag, hold then lift = right click, two fingers slide = scroll, pinch = zoom, three fingers tap = keyboard",
        pastePrompt: "Text to paste on the remote computer",
    };
    let lang = load("lang") || ((navigator.language || "en").toLowerCase().startsWith("fr") ? "fr" : "en");

    function t(key) {
        const table = I18N[lang] || {};
        if (key in table) return table[key];
        if (key in EN_TEXT) return EN_TEXT[key];
        return key;
    }

    // Touch screens: no native tooltip (a long press shows it and it stays on screen after the finger left), only the label
    /* No native tooltip on a touch screen (it shows on a tap and stays): some phones report a hover (stylus, HyperOS
     * WebView), so the app, a coarse pointer and the first touch turn them off too; every title is then kept as the
     * aria-label only, the ones added later included
    */
    let noTips = IN_APP || !!(window.matchMedia && (window.matchMedia("(hover: none)").matches || window.matchMedia("(any-pointer: coarse)").matches));

    function dropTips(root) {
        for (const el of root.querySelectorAll("[title]")) {
            if (!el.getAttribute("aria-label")) el.setAttribute("aria-label", el.title);
            el.removeAttribute("title");
        }
    }

    function tipsOff() {
        noTips = true;
        dropTips(document);
        new MutationObserver((list) => {
            for (const m of list) {
                if (m.type === "attributes" && m.target.hasAttribute("title")) dropTips(m.target.parentNode || document);
                else for (const n of m.addedNodes) if (n.nodeType === 1) {if (n.hasAttribute("title")) dropTips(n.parentNode || document); dropTips(n);}
            }
        }).observe(document.documentElement, {subtree: true, childList: true, attributes: true, attributeFilter: ["title"]});
    }

    function setTip(el, text) {
        el.setAttribute("aria-label", text);
        if (noTips) el.removeAttribute("title");
        else el.title = text;
    }

    document.addEventListener("pointerdown", (e) => {if (e.pointerType === "touch" && !noTips) tipsOff();}, true);
    if (noTips) tipsOff();

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
            setTip(el, tr !== undefined ? tr : el.dataset.i18nTitleEn);
        }
        if (noTips) dropTips(document);
        setStatus(state.status);
        if (typeof renderKeys === "function" && $("keysMain").childElementCount) renderKeys();
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
    function toast(text, ms) {
        const el = $("toast");
        el.textContent = text;
        el.hidden = false;
        clearTimeout(toastTimer);
        toastTimer = setTimeout(() => {el.hidden = true;}, ms || 3200);
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
        drawing: 0, // frames received, not drawn yet
        paused: false, // video stream paused (terminal tab, hidden page)
        frames: 0,
        bytes: 0,
        latency: -1,
        closedByUs: false,
        gotBye: false,
        terms: new Map(), // id -> {term, fit, el, tab, exited}
        activeTab: "screen",
        pressed: new Set(), // key codes down (released on blur)
        buttons: new Set(), // mouse buttons down
        session: "", // id given in the welcome
        lanes: [], // index -> extra websocket of the video (0 = the main one, never stored)
        laneOffer: null, // {token, max} of the welcome
        laneWanted: 1, // sockets the host asked for
        laneRetries: 0,
        tileIds: new Uint32Array(0), // frame id that drew each tile
        tileCols: 0,
        sizeId: 0, // frame that set the current size (an older one of another size is dropped)
    };

    // Android app: its own udp socket (same encrypted protocol as the native viewers), frames handed over a message port
    const bridge = {port: null, active: false, waited: false};

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

    // auto (follows the system) -> light -> dark -> auto
    const THEME_NEXT = {auto: "light", light: "dark", dark: "auto"};
    const THEME_LABEL = {auto: "Theme: auto (follows the system)", light: "Theme: light", dark: "Theme: dark"};

    function applyTheme(theme) {
        const choice = (theme === "light" || theme === "dark") ? theme : "auto";
        if (choice === "auto") delete document.documentElement.dataset.theme;
        else document.documentElement.dataset.theme = choice;
        document.documentElement.dataset.choice = choice;
        setTip($("themeBtn"), THEME_LABEL[choice]);
    }

    $("themeBtn").addEventListener("click", () => {
        const next = THEME_NEXT[document.documentElement.dataset.choice || "auto"];
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
        state.session = msg.session || "";
        state.laneOffer = msg.lanes && msg.lanes.token ? msg.lanes : null;
        state.laneWanted = 1;
        state.laneRetries = 0;
        state.tileIds = new Uint32Array(0);
        state.frameW = 0;
        state.frameH = 0;
        state.input = {state: "loading", error: ""}; // until the host says (its capture starts with the first viewer)
        if (msg.udp) bridgeSend({t: "start", offer: msg.udp});
        state.perms = Object.assign({view: false, control: false, terminal: false, clipboard: false}, msg.perms || {});
        setStatus("active");
        showView("sessionView");
        applyPerms();
        setMode(state.perms.control ? "control" : "view", true);
        updateScreenState();
        sendQuality();
        state.paused = false; // a new session streams until told otherwise
        state.drawing = 0;
        syncPause();
        $("screen").focus();
    }

    // ---------------------------------------------------------------- //
    // Connection

    function connect() {
        // The app hands its udp port right after the load: waited for once
        if (IN_APP && !bridge.port && !bridge.waited) {
            bridge.waited = true;
            setTimeout(connect, 500);
            return;
        }
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
            sendJson({t: "hello", v: PROTOCOL, kind: KIND, name: name, os: guessOs(), device: device ? device.id : "", nonce: b64encode(state.clientNonce), udp: !!bridge.port});
        };
        ws.onmessage = (e) => {
            if (typeof e.data === "string") onText(e.data);
            else onBinary(new Uint8Array(e.data));
        };
        ws.onclose = () => {
            if (state.ws !== ws) return;
            state.ws = null;
            closeLanes();
            bridgeSend({t: "stop"});
            releaseAll();
            if (state.closedByUs || state.gotBye) return;
            if (state.status === "active" || state.status === "pending" || state.status === "connecting") showClosed(t("closedTitle"), t("lostText"), false);
        };
    }

    function closeSocket() {
        closeLanes();
        bridgeSend({t: "stop"});
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

    // ---------------------------------------------------------------- //
    // Extra sockets of the video (asked by the host while one connection holds the frames back)

    function setLanes(n) {
        state.laneWanted = Math.max(1, Math.min(LANE_MAX, n | 0));
        for (let i = 1; i < LANE_MAX; ++i) {
            if (i < state.laneWanted) {
                if (!state.lanes[i]) openLane(i);
            } else if (state.lanes[i]) {
                const ws = state.lanes[i];
                state.lanes[i] = null;
                setTimeout(() => {try {ws.close(1000);} catch (e) { /* closed */ }}, LANE_RETIRE); // its last frames arrive
            }
        }
    }

    function openLane(index) {
        if (!state.laneOffer || state.status !== "active") return;
        const ws = new WebSocket("wss://" + location.host + "/lane");
        ws.binaryType = "arraybuffer";
        state.lanes[index] = ws;
        ws.onopen = () => ws.send(JSON.stringify({t: "lane", session: state.session, token: state.laneOffer.token, index: index}));
        ws.onmessage = (e) => {
            if (typeof e.data === "string") return;
            const data = new Uint8Array(e.data);
            state.bytes += data.length;
            if (data.length > 0 && data[0] === BIN.Frame) onFrame(data, ws);
        };
        ws.onclose = () => {
            if (state.lanes[index] !== ws) return;
            state.lanes[index] = null;
            // Still wanted: again a bit later (a few tries only)
            if (index < state.laneWanted && state.status === "active" && state.laneRetries < 5) {
                state.laneRetries += 1;
                setTimeout(() => {if (!state.lanes[index] && index < state.laneWanted) openLane(index);}, 1000);
            }
        };
    }

    function closeLanes() {
        const lanes = state.lanes;
        state.lanes = [];
        state.laneWanted = 1;
        for (const ws of lanes)
            if (ws) try {ws.close(1000);} catch (e) { /* closed */ }
    }

    function bridgeSend(obj) {
        if (bridge.port) bridge.port.postMessage(JSON.stringify(obj));
    }

    function base64Bytes(text) {
        if (Uint8Array.fromBase64) return Uint8Array.fromBase64(text);
        const bin = atob(text);
        const out = new Uint8Array(bin.length);
        for (let i = 0; i < bin.length; ++i)
            out[i] = bin.charCodeAt(i);
        return out;
    }

    // "F" + base64 frame (acked by the app over udp), "S" + json status
    function onBridge(text) {
        if (typeof text !== "string" || text.length < 2) return;
        if (text[0] === "F") {
            const data = base64Bytes(text.slice(1));
            state.bytes += data.length;
            if (state.status === "active" && data[0] === BIN.Frame) onFrame(data, null);
            return;
        }
        let msg;
        try {msg = JSON.parse(text.slice(1));} catch (e) {return;}
        if (typeof msg.active === "boolean") {
            bridge.active = msg.active;
            if (!msg.active && state.status === "active") sendJson({t: "udp_off"}); // unreachable or silent: the video stays on tcp
        }
        if (msg.gaveUp && state.status === "active") sendJson({t: "refresh"}); // a frame lost for good
    }

    if (IN_APP) {
        window.addEventListener("message", (e) => {
            if (e.data !== "xdesk-udp" || !e.ports || !e.ports[0] || bridge.port) return;
            bridge.port = e.ports[0];
            bridge.port.onmessage = (m) => onBridge(m.data);
        });
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
            case "input": onInputState(msg); break;
            case "lanes": setLanes(msg.n); break;
            case "term_exit": onTermExit(msg.id, msg.code); break;
            case "clip": if (msg.text && navigator.clipboard && document.hasFocus()) navigator.clipboard.writeText(msg.text).catch(() => {}); break;
            default: break;
        }
    }

    function onBinary(data) {
        state.bytes += data.length;
        if (data.length < 1) return;
        switch (data[0]) {
            case BIN.Frame: onFrame(data, state.ws); break;
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
        setMenu(false);
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
    // QR code scanner (BarcodeDetector when the browser has it, else jsQR loaded on demand)

    const scan = {stream: null, timer: 0, detector: null, canvas: null};

    function loadJsQR() {
        if (window.jsQR) return Promise.resolve();
        return new Promise((resolve, reject) => {
            const script = document.createElement("script");
            script.src = "/vendor/jsQR.js";
            script.onload = resolve;
            script.onerror = reject;
            document.head.appendChild(script);
        });
    }

    function stopScan() {
        clearTimeout(scan.timer);
        if (scan.stream) for (const track of scan.stream.getTracks()) track.stop();
        scan.stream = null;
        $("scanVideo").srcObject = null;
        $("scanView").hidden = true;
    }

    // xdesk://host:port?fp=..&code=.. (app link) or https://host:port/?code=.. (browser link)
    function onScanned(text) {
        let url = null;
        try {
            url = new URL(text.trim().replace(/^xdesk:\/\//i, "https://"));
        } catch (e) {
            url = null;
        }
        const code = url && url.protocol === "https:" ? url.searchParams.get("code") : null;
        stopScan();
        if (!code) {
            showConnect(t("scanNotXdesk"));
            return;
        }
        if (url.host !== location.host) {
            if (confirm(t("scanOther") + url.host)) location.href = "https://" + url.host + "/?code=" + encodeURIComponent(normalizeCode(code));
            return;
        }
        $("codeInput").value = formatCode(code);
        showConnect("");
        connect();
    }

    async function scanFrame() {
        const video = $("scanVideo");
        if (!scan.stream) return;
        if (video.readyState >= 2 && video.videoWidth > 0) {
            try {
                if (scan.detector) {
                    const found = await scan.detector.detect(video);
                    if (found.length) return onScanned(found[0].rawValue);
                } else {
                    const w = Math.min(640, video.videoWidth);
                    const h = Math.round(video.videoHeight * w / video.videoWidth);
                    const c = scan.canvas || (scan.canvas = document.createElement("canvas"));
                    c.width = w;
                    c.height = h;
                    const g = c.getContext("2d", {willReadFrequently: true});
                    g.drawImage(video, 0, 0, w, h);
                    const found = window.jsQR(g.getImageData(0, 0, w, h).data, w, h, {inversionAttempts: "dontInvert"});
                    if (found) return onScanned(found.data);
                }
            } catch (e) { /* next frame */ }
        }
        scan.timer = setTimeout(scanFrame, 150);
    }

    async function startScan() {
        if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
            showConnect(t("scanNoCamera"));
            return;
        }
        try {
            scan.detector = null;
            if ("BarcodeDetector" in window && (await window.BarcodeDetector.getSupportedFormats()).includes("qr_code")) scan.detector = new window.BarcodeDetector({formats: ["qr_code"]});
            else await loadJsQR();
            scan.stream = await navigator.mediaDevices.getUserMedia({video: {facingMode: {ideal: "environment"}}, audio: false});
        } catch (e) {
            stopScan();
            showConnect(t("scanNoCamera"));
            return;
        }
        $("scanView").hidden = false;
        $("scanVideo").srcObject = scan.stream;
        await $("scanVideo").play().catch(() => {});
        scanFrame();
    }

    $("scanBtn").addEventListener("click", startScan);
    $("scanClose").addEventListener("click", stopScan);
    document.addEventListener("keydown", (e) => {if (e.key === "Escape" && scan.stream) stopScan();});

    // ---------------------------------------------------------------- //
    // Frames

    const canvas = $("screen");
    const ctx = canvas.getContext("2d", {alpha: false, desynchronized: true});
    const cursorEl = $("cursor");
    let cursorPos = {x: -1, y: -1, visible: false};

    function onFrame(data, via) {
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
        // Ack at once while the drawing keeps up (the host encodes the next frame meanwhile), else once drawn; on the
        // socket that brought it (the app acks its udp frames itself)
        const ack = new Uint8Array(5);
        ack[0] = BIN.FrameAck;
        new DataView(ack.buffer).setUint32(1, id);
        const sendAck = () => {if (via && via.readyState === WebSocket.OPEN) via.send(ack);};
        const early = state.drawing < 1;
        if (early) sendAck();
        state.drawing += 1;

        // Decode in parallel, draw in order of arrival; a tile already drawn by a newer frame is kept
        const decoded = Promise.all(tiles.map((tl) => tl.blob ? createImageBitmap(tl.blob).catch(() => null) : null));
        state.drawQueue = state.drawQueue.then(() => decoded).then((bitmaps) => {
            if (w !== state.frameW || h !== state.frameH) {
                if (state.frameW && ((id - state.sizeId) | 0) < 0) bitmaps = []; // a late frame of the old size
                else {
                    state.frameW = w;
                    state.frameH = h;
                    state.sizeId = id;
                    updateScreenState();
                    state.tileCols = Math.ceil(w / TILE_SIZE);
                    state.tileIds = new Uint32Array(state.tileCols * Math.ceil(h / TILE_SIZE));
                    canvas.width = w;
                    canvas.height = h;
                    layoutCanvas();
                }
            }
            for (let i = 0; i < tiles.length && bitmaps.length; ++i) {
                const tl = tiles[i];
                const cells = newerCells(id, tl);
                if (tl.color) {
                    ctx.fillStyle = tl.color;
                    for (const c of cells.kept)
                        ctx.fillRect(c[0], c[1], c[2], c[3]);
                } else if (bitmaps[i]) {
                    if (cells.all) ctx.drawImage(bitmaps[i], tl.x, tl.y, tl.w, tl.h);
                    else for (const c of cells.kept)
                        ctx.drawImage(bitmaps[i], c[0] - tl.x, c[1] - tl.y, c[2], c[3], c[0], c[1], c[2], c[3]);
                }
            }
            for (const b of bitmaps)
                if (b) b.close();
            state.drawing -= 1;
            if (!early) sendAck();
            state.frames += 1;
            placeCursor();
        }).catch(() => {state.drawing = Math.max(0, state.drawing - 1); if (!early) sendAck();});
    }

    // Tiles of a frame not drawn yet by a newer one (any order of arrival), marked as drawn by this frame
    function newerCells(id, tl) {
        const kept = [];
        let all = true;
        const right = Math.min(tl.x + tl.w, state.frameW);
        const bottom = Math.min(tl.y + tl.h, state.frameH);
        for (let y = Math.floor(tl.y / TILE_SIZE) * TILE_SIZE; y < bottom; y += TILE_SIZE) {
            for (let x = Math.floor(tl.x / TILE_SIZE) * TILE_SIZE; x < right; x += TILE_SIZE) {
                const k = (y / TILE_SIZE) * state.tileCols + x / TILE_SIZE;
                const from = state.tileIds[k];
                if (from !== 0 && ((id - from) | 0) < 0) {all = false; continue;}
                state.tileIds[k] = id;
                const cx = Math.max(x, tl.x);
                const cy = Math.max(y, tl.y);
                kept.push([cx, cy, Math.min(x + TILE_SIZE, right) - cx, Math.min(y + TILE_SIZE, bottom) - cy]);
            }
        }
        return {all: all, kept: kept};
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
        // Trackpad: the local pointer (no round trip), else the remote one
        const pos = trackpad() && state.mode === "control" && pad.x >= 0 ? {x: pad.x, y: pad.y, visible: true} : cursorPos;
        const hide = !pos.visible || pos.x < 0 || !state.frameW || (state.mode === "control" && !touchMode);
        const box = hide ? null : drawnBox();
        cursorEl.hidden = !box;
        if (!box) return;
        const stage = $("stage").getBoundingClientRect();
        cursorEl.style.transform = "translate(" + (box.left - stage.left + pos.x * box.ratio) + "px," + (box.top - stage.top + pos.y * box.ratio) + "px)";
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
        requestAnimationFrame(applyZoom);
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
        updateKeybar();
        updateScreenState();
    }

    /* State of the screen and of the controls, over the screen:
     * no frame yet -> spinner "Loading the screen", control asked while the host opens its input -> "Getting the controls ready",
     * no input backend on the host -> its reason (kept on screen), ready -> "Controls ready" for a moment
    */
    let readyTimer = 0;

    function onInputState(msg) {
        const before = state.input ? state.input.state : "";
        state.input = {state: msg.state || "none", error: msg.error || ""};
        updateScreenState();
        if (state.input.state === "ready" && before !== "ready" && state.mode === "control") {
            const el = $("screenState");
            el.hidden = false;
            el.className = "screen-state ok";
            $("screenStateText").textContent = t("controlReady");
            clearTimeout(readyTimer);
            readyTimer = setTimeout(updateScreenState, 1500);
        }
    }

    function updateScreenState() {
        const el = $("screenState");
        const input = state.input || {state: "loading", error: ""};
        let kind = "";
        let text = "";

        clearTimeout(readyTimer);
        if (state.status !== "active" || !state.perms.view) kind = "";
        else if (!state.frameW) {kind = "loading"; text = t("screenLoading");}
        else if (state.mode === "control" && input.state === "loading") {kind = "loading"; text = t("controlLoading");}
        else if (state.mode === "control" && input.state === "paused") {kind = "warn"; text = t("controlPaused");}
        else if (state.mode === "control" && input.state === "none") {kind = "warn"; text = t("controlNone") + (input.error ? ": " + input.error : "");}
        el.hidden = !kind;
        el.className = "screen-state " + kind;
        $("screenStateText").textContent = text;
    }

    function setMode(mode, send) {
        if (mode === "control" && !state.perms.control) {
            toast(t("controlDenied"));
            return;
        }
        if (mode !== "control") releaseAll();
        state.mode = mode;
        if (typeof updateScreenState === "function" && state.status === "active") updateScreenState();
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
        if (state.status === "active" && bridge.active && state.latency >= 0) bridgeSend({t: "rtt", ms: state.latency}); // pace of its nacks
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
    const touches = new Map(); // pointerId -> {x, y, startX, startY}

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
        if (e.pointerType === "touch") {
            touches.delete(e.pointerId);
            if (gesture) clearTimeout(gesture.timer);
            if (touches.size === 0) gesture = null;
        }
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
    // Touch
    //  trackpad (default): one finger slides the pointer, tap = click, double tap = double click, tap then slide or hold then slide = drag
    //  direct: the finger is the pointer, tap = click there, double tap = double click, slide = drag, hold then lift = right click, hold then slide = drag
    //  both: the second tap of a double tap holds the button while the finger stays (double click + hold / slide, as with a mouse)
    //  both: two fingers tap = right click, two fingers slide = scroll, pinch = zoom, three fingers tap = keyboard

    const view = {zoom: 1, panX: 0, panY: 0}; // zoom of the screen (fit mode), css pixels
    const pad = {x: -1, y: -1}; // pointer moved by the trackpad (frame pixels)
    let gesture = null;
    let lastTap = null; // {t, x, y, p}: end of the last one finger tap (double taps)

    function touchInput() {return load("touch") === "direct" ? "direct" : "trackpad";}
    function trackpad() {return touchMode && touchInput() === "trackpad";}

    function markTouch() {
        if (touchMode && document.documentElement.classList.contains("touch")) return;
        touchMode = true;
        document.documentElement.classList.add("touch");
        updateKeybar();
    }

    function applyZoom() {
        if (view.zoom <= 1.01 || !$("viewport").classList.contains("fit")) {view.zoom = 1; view.panX = 0; view.panY = 0;}
        const w = canvas.clientWidth;
        const h = canvas.clientHeight;
        view.panX = Math.min(0, Math.max(w - w * view.zoom, view.panX));
        view.panY = Math.min(0, Math.max(h - h * view.zoom, view.panY));
        canvas.style.transformOrigin = "0 0";
        canvas.style.transform = view.zoom > 1 ? "translate(" + view.panX + "px," + view.panY + "px) scale(" + view.zoom + ")" : "";
        placeCursor();
    }

    // Zoomed: the screen follows the trackpad pointer
    function followPad() {
        const box = drawnBox();
        if (view.zoom <= 1 || !box) return;
        const vp = $("viewport").getBoundingClientRect();
        const x = box.left + pad.x * box.ratio;
        const y = box.top + pad.y * box.ratio;
        const margin = Math.min(60, vp.width / 5, vp.height / 5);
        if (x < vp.left + margin) view.panX += vp.left + margin - x;
        else if (x > vp.right - margin) view.panX -= x - (vp.right - margin);
        if (y < vp.top + margin) view.panY += vp.top + margin - y;
        else if (y > vp.bottom - margin) view.panY -= y - (vp.bottom - margin);
        applyZoom();
    }

    // Trackpad pointer: from the remote one, else the middle of the screen
    function padStart() {
        if (pad.x >= 0 && pad.x < state.frameW && pad.y >= 0 && pad.y < state.frameH) return;
        if (cursorPos.visible && cursorPos.x >= 0) {pad.x = cursorPos.x; pad.y = cursorPos.y;}
        else {pad.x = state.frameW / 2; pad.y = state.frameH / 2;}
    }

    function padPoint() {return {x: Math.round(pad.x), y: Math.round(pad.y)};}

    function click(button, p) {
        if (p) sendMove(p);
        sendButton(button, true);
        sendButton(button, false);
    }

    function twoFingers() {
        const list = Array.from(touches.values());
        return {d: Math.hypot(list[0].x - list[1].x, list[0].y - list[1].y), x: (list[0].x + list[1].x) / 2, y: (list[0].y + list[1].y) / 2};
    }

    function onTouchDown(e) {
        markTouch();
        if (!state.perms.view) return;
        e.preventDefault();
        canvas.setPointerCapture(e.pointerId);
        const now = performance.now();
        touches.set(e.pointerId, {x: e.clientX, y: e.clientY, startX: e.clientX, startY: e.clientY});
        if (touches.size === 1) {
            const chained = lastTap && now - lastTap.t < DOUBLE_TAP && Math.hypot(e.clientX - lastTap.x, e.clientY - lastTap.y) < DOUBLE_SLOP * 3;
            gesture = {t: now, last: now, fingers: 1, moved: false, long: false, dragging: false, timer: 0, kind: "", two: null, scrollX: 0, scrollY: 0, p: null, chained: chained};
            if (!canControl()) return;
            if (trackpad()) {
                padStart();
                placeCursor();
                if (chained) {
                    // Second tap: the button follows the finger (real double click, tap then slide = drag)
                    gesture.dragging = true;
                    sendMove(padPoint());
                    sendButton(0, true);
                    return;
                }
                gesture.timer = setTimeout(() => {
                    // Held still: the left button stays down, the finger drags
                    gesture.long = true;
                    gesture.dragging = true;
                    sendMove(padPoint());
                    sendButton(0, true);
                    if (navigator.vibrate) navigator.vibrate(15);
                }, LONG_PRESS);
            } else {
                // Second tap close to the first one: the same point (a finger never lands twice on the same pixel of the host)
                const near = chained && Math.hypot(e.clientX - lastTap.x, e.clientY - lastTap.y) < DOUBLE_SLOP;
                gesture.p = near ? lastTap.p : toFrame(e.clientX, e.clientY);
                if (gesture.p) sendMove(gesture.p);
                if (near) {
                    // Second tap: the button follows the finger like a mouse (double click, double click + hold / slide = select by words)
                    gesture.dragging = true;
                    sendButton(0, true);
                    return;
                }
                gesture.timer = setTimeout(() => {
                    // Held still: right click when lifted, drag when it slides
                    gesture.long = true;
                    if (navigator.vibrate) navigator.vibrate(15);
                }, LONG_PRESS);
            }
            return;
        }
        if (!gesture) return;
        clearTimeout(gesture.timer);
        gesture.fingers = Math.max(gesture.fingers, touches.size);
        if (gesture.dragging) {sendButton(0, false); gesture.dragging = false;}
        if (touches.size === 2) {
            // Untransformed position of the canvas: the pinch keeps the point under the fingers
            const two = twoFingers();
            const r = canvas.getBoundingClientRect();
            gesture.two = {d: two.d, x: two.x, y: two.y, zoom: view.zoom, left: r.left - view.panX, top: r.top - view.panY, panX: view.panX, panY: view.panY};
            gesture.kind = "";
        }
    }

    function onTouchMove(e) {
        const tp = touches.get(e.pointerId);
        if (!tp || !gesture) return;
        e.preventDefault();
        const dx = e.clientX - tp.x;
        const dy = e.clientY - tp.y;
        const now = performance.now();
        const dt = Math.max(1, now - gesture.last);
        tp.x = e.clientX;
        tp.y = e.clientY;
        gesture.last = now;
        if (Math.hypot(e.clientX - tp.startX, e.clientY - tp.startY) > TAP_SLOP) {
            gesture.moved = true;
            if (!gesture.long) clearTimeout(gesture.timer);
        }
        if (touches.size >= 2) {twoFingerMove(dx, dy); return;}
        if (gesture.fingers > 1) return; // the fingers of a two-finger gesture leaving one by one
        if (!canControl()) {
            // View only: one finger moves the zoomed screen
            if (view.zoom > 1) {view.panX += dx; view.panY += dy; applyZoom();}
            return;
        }
        if (trackpad()) {
            const box = drawnBox();
            if (!box || !gesture.moved) return; // the jitter of a tap / hold never moves the pointer (double clicks stay on one point)
            const accel = 1 + Math.min(2.5, Math.hypot(dx, dy) / dt * 0.9); // a fast slide goes further
            pad.x = Math.max(0, Math.min(state.frameW - 1, pad.x + dx / box.ratio * accel));
            pad.y = Math.max(0, Math.min(state.frameH - 1, pad.y + dy / box.ratio * accel));
            sendMove(padPoint());
            followPad();
            placeCursor();
            return;
        }
        if (gesture.moved && !gesture.dragging) {
            gesture.dragging = true;
            const start = gesture.p || toFrame(tp.startX, tp.startY);
            if (start) sendMove(start);
            sendButton(0, true);
        }
        if (gesture.dragging && gesture.moved) {
            const p = toFrame(e.clientX, e.clientY);
            if (p) sendMove(p);
        }
    }

    function twoFingerMove(dx, dy) {
        const g = gesture.two;
        if (!g) return;
        const two = twoFingers();
        if (!gesture.kind) {
            // Fingers spreading = zoom, moving together = scroll
            const spread = Math.abs(two.d - g.d);
            const shift = Math.hypot(two.x - g.x, two.y - g.y);
            if (spread > 14 && spread > shift * 0.6 && $("viewport").classList.contains("fit")) gesture.kind = "pinch";
            else if (shift > 10) gesture.kind = "scroll";
            else return;
        }
        if (gesture.kind === "pinch") {
            const zoom = Math.max(1, Math.min(MAX_ZOOM, g.zoom * two.d / Math.max(1, g.d)));
            const localX = (g.x - g.left - g.panX) / g.zoom;
            const localY = (g.y - g.top - g.panY) / g.zoom;
            view.zoom = zoom;
            view.panX = two.x - g.left - localX * zoom;
            view.panY = two.y - g.top - localY * zoom;
            applyZoom();
            return;
        }
        // Scroll: the remote wheel when controlling, else the zoomed screen moves
        if (!canControl()) {
            if (view.zoom > 1) {view.panX += dx / touches.size; view.panY += dy / touches.size; applyZoom();}
            return;
        }
        gesture.scrollY += dy / touches.size;
        gesture.scrollX += dx / touches.size;
        const sy = Math.trunc(gesture.scrollY / 4);
        const sx = Math.trunc(gesture.scrollX / 4);
        if (sy || sx) {
            sendWheel(sx * 12, sy * 12);
            gesture.scrollY -= sy * 4;
            gesture.scrollX -= sx * 4;
        }
    }

    function onTouchUp(e) {
        const tp = touches.get(e.pointerId);
        touches.delete(e.pointerId);
        if (!tp || !gesture) return;
        e.preventDefault();
        clearTimeout(gesture.timer);
        if (touches.size > 0) return; // the gesture ends with its last finger
        const g = gesture;
        const now = performance.now();
        const quick = now - g.t < TAP_TIME;
        gesture = null;
        lastTap = null;
        if (g.dragging) {sendButton(0, false); return;}
        if (g.kind) return;
        if (g.fingers >= 3) {if (quick) toggleKeyboard(); return;}
        if (!canControl()) return;
        if (g.fingers === 2) {if (quick) click(2, trackpad() ? padPoint() : toFrame(tp.startX, tp.startY)); return;}
        if (g.moved) return;
        const p = trackpad() ? padPoint() : (g.p || toFrame(tp.startX, tp.startY));
        if (g.long) {if (!trackpad()) click(2, p); return;} // direct: held still then lifted = right click
        click(0, p);
        if (!g.chained) lastTap = {t: now, x: tp.startX, y: tp.startY, p: p}; // a third tap starts again
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
    document.addEventListener("visibilitychange", () => {
        if (document.hidden) releaseAll();
        syncPause();
    });

    // No video while it cannot be seen (terminal tab, hidden page): the terminal bytes never wait behind frames
    function syncPause() {
        const paused = document.hidden || state.activeTab !== "screen";
        if (paused === state.paused) return;
        state.paused = paused;
        sendJson({t: "pause", paused: paused});
    }

    $("cadBtn").addEventListener("click", sendCad);

    // On-screen keyboard: a hidden input opens the soft keyboard, its text is sent as Text messages
    $("kbdBtn").addEventListener("click", toggleKeyboard);

    /* Soft keyboard: the input keeps what was typed and only the difference with what the host already got is sent
     * (prediction, autocorrection, a suggestion tapped: the keyboard rewrites its word, the host gets the backspaces and
     * the new letters, never the whole text again); the screen gets text / keys, a terminal its bytes
    */
    const KBD_KEEP = 200; // characters kept before the input is emptied (between two words)
    let kbdSent = ""; // what the host has of the input text
    let kbdComposing = false;

    // Where the typed text goes: "screen", the active terminal, or nowhere
    function kbdTarget() {
        if (state.activeTab === "screen") return canControl() ? "screen" : null;
        const tm = activeTerm();
        return tm && !tm.exited ? tm : null;
    }

    function kbdReset() {
        kbdInput.value = "";
        kbdSent = "";
    }

    function kbdBackspaces(target, n) {
        if (n <= 0) return;
        if (target !== "screen") {sendTermBytes(target, "\x7f".repeat(n)); return;}
        for (let i = 0; i < n; ++i) {sendKey("Backspace", true); sendKey("Backspace", false);}
    }

    function kbdText(target, text) {
        if (!text) return;
        if (target !== "screen") {sendTermBytes(target, termWithMods(text)); return;}
        if (!typedWithMods(text)) {sendText(text); releaseOneShot();}
    }

    function kbdSync() {
        const target = kbdTarget();
        const value = kbdInput.value;
        if (!target) {kbdSent = value; return;}
        if (value === kbdSent) return;
        const before = Array.from(kbdSent);
        const now = Array.from(value);
        let same = 0;
        while (same < before.length && same < now.length && before[same] === now[same]) ++same;
        kbdSent = value;
        kbdBackspaces(target, before.length - same);
        kbdText(target, now.slice(same).join(""));
        if (!kbdComposing && now.length > KBD_KEEP && value.endsWith(" ")) kbdReset();
    }

    kbdInput.addEventListener("beforeinput", (e) => {
        const target = kbdTarget();
        if (!target) return;
        // Nothing left in the input: the deletion is for the text typed before (the host side)
        if ((e.inputType === "deleteContentBackward" || e.inputType === "deleteWordBackward") && !kbdInput.value) {e.preventDefault(); kbdBackspaces(target, 1);}
        else if (e.inputType === "insertLineBreak" || e.inputType === "insertParagraph") {e.preventDefault(); pressKey("Enter"); kbdReset();}
    });

    kbdInput.addEventListener("input", kbdSync); // composing too: every letter reaches the host at once
    kbdInput.addEventListener("compositionstart", () => {kbdComposing = true;});
    kbdInput.addEventListener("compositionend", () => {kbdComposing = false; kbdSync();});
    kbdInput.addEventListener("focus", kbdReset);
    kbdInput.addEventListener("blur", kbdReset);

    kbdInput.addEventListener("keydown", (e) => {
        const target = kbdTarget();
        if (!target || e.isComposing || e.keyCode === 229) return; // soft keyboard: the input events carry it
        // Hardware keys while the hidden input has the focus (a keyboard plugged in the phone)
        const special = ["Enter", "Escape", "Tab", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Home", "End", "PageUp", "PageDown", "Delete", "Insert"];
        if (e.key === "Backspace" && !kbdInput.value) {e.preventDefault(); kbdBackspaces(target, 1); return;}
        if ((e.ctrlKey || e.altKey || e.metaKey) && e.key.length === 1) {
            e.preventDefault();
            const saved = Object.assign({}, mods);
            if (e.ctrlKey) mods.ctrl = 1;
            if (e.altKey) mods.alt = 1;
            if (e.metaKey) mods.meta = 1;
            if (target === "screen") typedWithMods(e.key.toLowerCase()) || pressKey(e.code);
            else sendTermBytes(target, termWithMods(e.key));
            Object.assign(mods, saved);
            renderMods();
            return;
        }
        if (!special.includes(e.key)) return;
        e.preventDefault();
        pressKey(e.code || e.key);
        kbdReset(); // the keyboard's word is no longer before the remote caret
    });

    // ---------------------------------------------------------------- //
    // Special keys (phones and tablets, or asked with the keys button)
    //  a modifier: one tap = the next key only, a second tap = locked, one more = off

    const MODIFIERS = {ctrl: "ControlLeft", alt: "AltLeft", shift: "ShiftLeft", meta: "MetaLeft"};
    const mods = {ctrl: 0, alt: 0, shift: 0, meta: 0}; // 0 off, 1 next key, 2 locked
    const modTaps = {};
    const repeat = {timer: 0, interval: 0, fired: false};
    const KBD_ICON = '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="2.5" y="6" width="19" height="12" rx="2" fill="none" stroke="currentColor" stroke-width="2"/><path d="M6 10h1M10 10h1M14 10h1M18 10h0M7 14h10" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>';
    const MAIN_KEYS = [
        {act: "input", screen: true},
        {act: "kbd", html: KBD_ICON, title: "keyboard"},
        {code: "Escape", label: "Esc"},
        {code: "Tab", label: "Tab"},
        {mod: "ctrl", label: "Ctrl", gap: true},
        {mod: "alt", label: "Alt"},
        {mod: "shift", label: "Shift"},
        {mod: "meta", label: "Win", press: "MetaLeft"}, // tap = the key at once (menu / activities), hold = sticky for a combination
        {code: "ArrowLeft", label: "←", repeat: true, gap: true},
        {code: "ArrowUp", label: "↑", repeat: true},
        {code: "ArrowDown", label: "↓", repeat: true},
        {code: "ArrowRight", label: "→", repeat: true},
        {code: "Home", label: "Home", gap: true},
        {code: "End", label: "End"},
        {code: "PageUp", label: "PgUp", repeat: true},
        {code: "PageDown", label: "PgDn", repeat: true},
        {code: "Backspace", label: "⌫", repeat: true, gap: true},
        {code: "Delete", label: "Del", repeat: true},
        {code: "Enter", label: "⏎"},
        {act: "fn", label: "Fn", gap: true},
        {act: "paste", label: "paste"},
        {act: "cad", label: "Ctrl+Alt+Del", screen: true},
    ];
    const FN_KEYS = ["F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"].map((f) => ({code: f, label: f}))
        .concat([{code: "Insert", label: "Ins", gap: true}, {code: "PrintScreen", label: "PrtSc", screen: true}]);
    // Terminal bytes: letter = CSI / SS3 final byte (cursor keys, F1-F4), else the whole sequence
    const TERM_KEYS = {
        ArrowUp: "A", ArrowDown: "B", ArrowRight: "C", ArrowLeft: "D", Home: "H", End: "F", F1: "P", F2: "Q", F3: "R", F4: "S",
        Escape: "\x1b", Tab: "\t", Enter: "\r", Backspace: "\x7f", Insert: "\x1b[2~", Delete: "\x1b[3~", PageUp: "\x1b[5~", PageDown: "\x1b[6~",
        F5: "\x1b[15~", F6: "\x1b[17~", F7: "\x1b[18~", F8: "\x1b[19~", F9: "\x1b[20~", F10: "\x1b[21~", F11: "\x1b[23~", F12: "\x1b[24~",
    };
    const CHAR_CODES = {" ": "Space", "-": "Minus", "=": "Equal", "[": "BracketLeft", "]": "BracketRight", ";": "Semicolon", "'": "Quote", ",": "Comma", ".": "Period", "/": "Slash", "\\": "Backslash", "`": "Backquote"};

    function activeTerm() {
        return state.activeTab.startsWith("term") ? state.terms.get(parseInt(state.activeTab.slice(4), 10)) : null;
    }

    function ensureControl() {
        if (canControl()) return true;
        if (!state.perms.control) {toast(t("controlDenied")); return false;}
        setMode("control", true);
        return canControl();
    }

    function termSequence(code, tm) {
        const seq = TERM_KEYS[code];
        const m = 1 + (mods.shift ? 1 : 0) + (mods.alt ? 2 : 0) + (mods.ctrl ? 4 : 0);
        if (!seq) return "";
        if (seq.length === 1 && /[A-DHFP-S]/.test(seq)) {
            if (m > 1) return "\x1b[1;" + m + seq;
            const app = /[A-DHF]/.test(seq) && tm.term.modes && tm.term.modes.applicationCursorKeysMode;
            return (app || /[P-S]/.test(seq) ? "\x1bO" : "\x1b[") + seq;
        }
        if (seq.endsWith("~") && m > 1) return seq.slice(0, -1) + ";" + m + "~";
        if (code === "Tab" && mods.shift) return "\x1b[Z";
        if (code === "Backspace" && mods.ctrl) return (mods.alt ? "\x1b" : "") + "\x08";
        return (mods.alt ? "\x1b" : "") + seq;
    }

    function sendTermBytes(tm, text) {
        if (!text || tm.exited) return;
        sendBin(concat(new Uint8Array([BIN.TermData, tm.id]), new TextEncoder().encode(text)));
    }

    // A key with the modifiers that are on (the screen: key events, a terminal: its bytes)
    function pressKey(code) {
        if (state.activeTab === "screen") {
            if (!ensureControl()) return;
            const held = Object.keys(mods).filter((name) => mods[name]).map((name) => MODIFIERS[name]);
            for (const m of held) sendKey(m, true);
            sendKey(code, true);
            sendKey(code, false);
            for (const m of held.reverse()) sendKey(m, false);
        } else {
            const tm = activeTerm();
            if (tm) sendTermBytes(tm, termSequence(code, tm));
        }
        releaseOneShot();
    }

    function sendCad() {
        if (!ensureControl()) return;
        sendKey("ControlLeft", true);
        sendKey("AltLeft", true);
        sendKey("Delete", true);
        sendKey("Delete", false);
        sendKey("AltLeft", false);
        sendKey("ControlLeft", false);
    }

    function tapModifier(name) {
        const now = performance.now();
        if (mods[name] === 1 && now - (modTaps[name] || 0) < 400) mods[name] = 2;
        else mods[name] = mods[name] ? 0 : 1;
        modTaps[name] = now;
        renderMods();
    }

    function releaseOneShot() {
        let changed = false;
        for (const name of Object.keys(mods)) if (mods[name] === 1) {mods[name] = 0; changed = true;}
        if (changed) renderMods();
    }

    function renderMods() {
        for (const b of document.querySelectorAll("#keybar .key[data-mod]")) {
            b.classList.toggle("on", mods[b.dataset.mod] === 1);
            b.classList.toggle("lock", mods[b.dataset.mod] === 2);
            b.setAttribute("aria-pressed", String(mods[b.dataset.mod] > 0));
        }
    }

    // Soft keyboard text while ctrl / alt / win is on: sent as the key (ctrl+c, alt+f...)
    function typedWithMods(text) {
        if (!(mods.ctrl || mods.alt || mods.meta) || Array.from(text).length !== 1) return false;
        const code = /^[a-z]$/i.test(text) ? "Key" + text.toUpperCase() : (/^[0-9]$/.test(text) ? "Digit" + text : (CHAR_CODES[text] || ""));
        if (!code) return false;
        pressKey(code);
        return true;
    }

    // Same for a terminal: control characters, alt = escape first
    function termWithMods(data) {
        if (!(mods.ctrl || mods.alt) || Array.from(data).length !== 1) return data;
        let out = data;
        if (mods.ctrl) {
            const c = data.toUpperCase().charCodeAt(0);
            if (data === " ") out = "\x00";
            else if (data === "?") out = "\x7f";
            else if (c >= 64 && c <= 95) out = String.fromCharCode(c - 64);
        }
        if (mods.alt) out = "\x1b" + out;
        releaseOneShot();
        return out;
    }

    function toggleKeyboard() {
        if (state.activeTab === "screen") {
            if (document.activeElement === kbdInput) {kbdInput.blur(); return;}
            if (!ensureControl()) return;
            kbdInput.value = "";
            kbdInput.focus();
            return;
        }
        const tm = activeTerm();
        if (!tm) return;
        const area = tm.el.querySelector("textarea");
        if (document.activeElement === kbdInput || (area && document.activeElement === area)) document.activeElement.blur();
        else if (touchMode) kbdInput.focus({preventScroll: true});
        else tm.term.focus();
    }

    function toggleInput() {
        save("touch", touchInput() === "trackpad" ? "direct" : "trackpad");
        pad.x = -1;
        if (touchInput() === "trackpad") padStart();
        renderKeys();
        placeCursor();
        toast(t(touchInput() === "trackpad" ? "helpTrackpad" : "helpDirect"), 7000);
    }

    async function pasteClipboard() {
        let text = "";
        try {
            text = await navigator.clipboard.readText();
        } catch (e) {
            text = window.prompt(t("pastePrompt"), "") || ""; // clipboard refused (permission, app): typed or pasted by hand
        }
        if (!text) return;
        if (state.activeTab === "screen") {
            if (ensureControl()) sendText(text);
            return;
        }
        const tm = activeTerm();
        if (tm && !tm.exited) tm.term.paste(text); // bracketed when the program asked for it
    }

    function keyButton(k) {
        const b = document.createElement("button");
        b.type = "button";
        b.className = "key" + (k.gap ? " gap" : "");
        if (k.code) b.dataset.code = k.code;
        if (k.mod) {b.dataset.mod = k.mod; b.setAttribute("aria-pressed", "false");}
        if (k.press) b.dataset.press = k.press;
        if (k.act) b.dataset.act = k.act;
        if (k.repeat) b.dataset.repeat = "1";
        if (k.screen) b.dataset.screen = "1";
        if (k.html) b.innerHTML = k.html;
        else b.textContent = k.label || "";
        if (k.title) b.dataset.title = k.title;
        return b;
    }

    function renderKeys() {
        const screen = state.activeTab === "screen";
        for (const b of document.querySelectorAll("#keybar .key")) {
            if (b.dataset.screen) b.hidden = !screen;
            if (b.dataset.act === "input") b.textContent = t(touchInput() === "trackpad" ? "trackpad" : "direct");
            if (b.dataset.act === "paste") b.textContent = t("paste");
            if (b.dataset.act === "fn") b.classList.toggle("on", !$("keysFn").hidden);
            if (b.dataset.title) setTip(b, t(b.dataset.title));
        }
        renderMods();
    }

    function keysWanted() {
        const v = load("keys");
        return v === null ? touchMode || window.matchMedia("(pointer: coarse)").matches : v === "1";
    }

    function updateKeybar() {
        const want = keysWanted();
        const show = want && state.status === "active" && (state.activeTab !== "screen" || state.perms.control);
        $("keysBtn").classList.toggle("on", want);
        $("keysBtn").setAttribute("aria-pressed", String(want));
        if ($("keybar").hidden === !show) {renderKeys(); return;}
        $("keybar").hidden = !show;
        renderKeys();
        requestAnimationFrame(() => {layoutCanvas(); fitActiveTerm();});
    }

    function activate(b) {
        const d = b.dataset;
        if (d.press && mods[d.mod]) {mods[d.mod] = 0; renderMods();} // sticky (held before): a tap turns it off
        else if (d.press) pressKey(d.press); // a tap sends the key itself (with the other sticky modifiers)
        else if (d.mod) tapModifier(d.mod);
        else if (d.code) pressKey(d.code);
        else if (d.act === "fn") {$("keysFn").hidden = !$("keysFn").hidden; renderKeys(); requestAnimationFrame(() => {layoutCanvas(); fitActiveTerm();});}
        else if (d.act === "kbd") toggleKeyboard();
        else if (d.act === "input") toggleInput();
        else if (d.act === "cad") sendCad();
        else if (d.act === "paste") pasteClipboard();
    }

    function stopRepeat() {
        clearTimeout(repeat.timer);
        clearInterval(repeat.interval);
    }

    for (const k of MAIN_KEYS) $("keysMain").appendChild(keyButton(k));
    for (const k of FN_KEYS) $("keysFn").appendChild(keyButton(k));

    // The focus (and the soft keyboard) stays where it is: the keys never take it
    $("keybar").addEventListener("mousedown", (e) => {if (e.target.closest(".key")) e.preventDefault();});
    $("keybar").addEventListener("click", (e) => {
        const b = e.target.closest(".key");
        if (!b) return;
        if (repeat.fired) {repeat.fired = false; return;} // held: already sent
        activate(b);
    });
    // Held arrows / erase keys repeat (a slide of the row cancels it)
    $("keybar").addEventListener("pointerdown", (e) => {
        const b = e.target.closest(".key");
        stopRepeat();
        repeat.fired = false;
        if (b && b.dataset.press) {
            // Held: sticky for the next key (win + e...), a second hold locks it
            repeat.timer = setTimeout(() => {
                repeat.fired = true;
                if (mods[b.dataset.mod] === 1) mods[b.dataset.mod] = 2;
                else mods[b.dataset.mod] = mods[b.dataset.mod] ? 0 : 1;
                renderMods();
                if (navigator.vibrate) navigator.vibrate(15);
            }, LONG_PRESS);
            return;
        }
        if (!b || !b.dataset.repeat) return;
        repeat.timer = setTimeout(() => {
            repeat.fired = true;
            activate(b);
            repeat.interval = setInterval(() => activate(b), 60);
        }, 450);
    });
    for (const name of ["pointerup", "pointercancel", "pointerleave"]) $("keybar").addEventListener(name, stopRepeat);

    $("keysBtn").addEventListener("click", () => {
        save("keys", keysWanted() ? "0" : "1");
        updateKeybar();
    });

    // ---------------------------------------------------------------- //
    // Tools menu (small screens)

    function setMenu(open) {
        $("toolsMenu").classList.toggle("open", open);
        $("menuBtn").setAttribute("aria-expanded", String(open));
    }

    $("menuBtn").addEventListener("click", () => setMenu(!$("toolsMenu").classList.contains("open")));
    document.addEventListener("pointerdown", (e) => {
        if ($("toolsMenu").classList.contains("open") && !e.target.closest("#toolsMenu, #menuBtn")) setMenu(false);
    });
    document.addEventListener("keydown", (e) => {if (e.key === "Escape" && $("toolsMenu").classList.contains("open")) setMenu(false);});

    // ---------------------------------------------------------------- //
    // Terminals (xterm.js)

    function selectTab(name) {
        state.activeTab = name;
        for (const b of document.querySelectorAll("#tabs .tab[data-tab]")) b.classList.toggle("active", b.dataset.tab === name);
        $("screenPane").hidden = name !== "screen";
        $("termPane").hidden = name === "screen";
        $("screenTools").hidden = name !== "screen";
        if (name !== "screen") releaseAll();
        syncPause();
        for (const [id, tm] of state.terms) tm.el.hidden = "term" + id !== name;
        updateKeybar();
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
        // Smallest free id: 1 again once the terminals are closed or after a reconnection
        let id = 1;
        while (id < 255 && state.terms.has(id)) ++id;

        const el = document.createElement("div");
        el.className = "term";
        $("termPane").appendChild(el);
        const term = new Terminal({
            cursorBlink: true,
            // Nerd / powerline fonts first when the device has one (prompt icons), then the system monospace ones
            fontFamily: "'JetBrainsMono Nerd Font', 'MesloLGS NF', 'FiraCode Nerd Font', 'Hack Nerd Font', 'CaskaydiaCove Nerd Font', 'Symbols Nerd Font Mono', ui-monospace, SFMono-Regular, Menlo, Consolas, 'DejaVu Sans Mono', 'Noto Sans Mono', monospace",
            fontSize: window.innerWidth < 600 ? 12 : 14,
            scrollback: 5000,
            allowProposedApi: true, // unicode 11 widths
            customGlyphs: true, // box drawing and block characters drawn by the renderer (no gap, no missing glyph)
            drawBoldTextInBrightColors: true,
            minimumContrastRatio: 1,
            theme: {background: "#0b0d0f", foreground: "#e4e7e9", cursor: "#3ddc97", selectionBackground: "#3ddc9755"},
        });
        const fit = new FitAddon.FitAddon();
        term.loadAddon(fit);
        if (window.Unicode11Addon) {
            term.loadAddon(new Unicode11Addon.Unicode11Addon());
            term.unicode.activeVersion = "11"; // emoji and wide symbols take 2 cells like in the real terminal
        }
        term.open(el);
        // GPU renderer (much faster than the DOM one on big outputs), the DOM one when WebGL is missing or lost
        if (window.WebglAddon) {
            try {
                const webgl = new WebglAddon.WebglAddon();
                webgl.onContextLoss(() => webgl.dispose());
                term.loadAddon(webgl);
            } catch (e) {
                /* DOM renderer */
            }
        }

        // Touch: the soft keyboard types through the hidden input (xterm's own one sends a whole word again when a suggestion is tapped)
        el.addEventListener("pointerdown", (e) => {if (e.pointerType === "touch") markTouch();}, true);
        if (term.textarea) term.textarea.addEventListener("focus", () => {if (touchMode) kbdInput.focus({preventScroll: true});});

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
            sendBin(concat(new Uint8Array([BIN.TermData, id]), encoder.encode(termWithMods(d))));
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
    $("fpsSelect").value = load("fps") || "60"; // the flow control lowers it by itself when the link cannot follow
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
