/*
 * xdesk android application: a WebView of the web client served by the host, with:
 *  - the host address asked once (or taken from a xdesk://host:port?fp=..&code=.. link)
 *  - certificate pinning: the sha256 of the host certificate is shown and confirmed at the first connection (or checked
 *    against the fp of the link), then every TLS error is accepted ONLY for that exact certificate
 *  - automatic updates: /app/version of the host compared with the installed version, /app/xdesk.apk installed with
 *    the PackageInstaller (user confirmation of the system)
 *  - video over the encrypted udp of the native viewers (UdpVideo): the page gets a message port right after its load,
 *    hands it the keys of its welcome, and gets the frames back from it (faster than its websocket on lossy links)
 *  - QR codes: a scanner page of the APK (assets/scan.html, served on a local https origin) reads the xdesk:// link shown
 *    by the host; the camera is only given to that page and to the pinned host (scanner of the web client)
*/
package dev.tsukini.xdesk;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.net.http.SslError;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.Base64;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.webkit.PermissionRequest;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebMessage;
import android.webkit.WebMessagePort;
import android.webkit.WebResourceResponse;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

import org.json.JSONObject;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

public class MainActivity extends Activity {
    // ---------------------------------------------------------------- //
    // Constants

    private static final String PREFS = "xdesk";
    private static final int DEFAULT_PORT = 47800;
    private static final int TIMEOUT = 8000; // ms
    private static final int RECENT_MAX = 5; // addresses kept on the connect screen
    private static final int CAMERA_REQUEST = 7;
    private static final String SCANNER = "https://appassets.androidplatform.net"; // local origin of the scanner page (served from the assets)

    /* palette (html-style dark tokens) */
    private static final int BG = Color.parseColor("#101315");
    private static final int SURFACE = Color.parseColor("#161a1d");
    private static final int TEXT = Color.parseColor("#e4e7e9");
    private static final int MUTED = Color.parseColor("#8d969d");
    private static final int ACCENT = Color.parseColor("#3ddc97");
    private static final int WARN = Color.parseColor("#f0a742");

    // ---------------------------------------------------------------- //
    // State

    private final Handler _ui = new Handler(Looper.getMainLooper());
    private SharedPreferences _prefs;
    private FrameLayout _root;
    private WebView _web;
    private String _pendingCode = ""; // code of a xdesk:// link, given to the web client once
    private boolean _updateChecked = false;
    private boolean _scanning = false; // the scanner page is shown (back = setup)
    private Runnable _cameraGranted; // waiting for the answer of the camera permission
    private Runnable _cameraDenied;
    private UdpVideo _udp; // video over udp of the current page
    private WebMessagePort _udpPort; // its side of the channel with the page

    // ---------------------------------------------------------------- //
    // Life cycle

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        this._prefs = this.getSharedPreferences(PREFS, MODE_PRIVATE);
        try {
            PackageInfo info = this.getPackageManager().getPackageInfo(this.getPackageName(), 0);
            BuildInfo.VERSION = info.versionName == null ? "0" : info.versionName;
        } catch (Exception e) {
            BuildInfo.VERSION = "0";
        }
        this.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        this.getWindow().setStatusBarColor(BG);
        this.getWindow().setNavigationBarColor(BG);

        this._root = new FrameLayout(this);
        this._root.setBackgroundColor(BG);
        // Edge to edge (target 35+): the content is padded by the system bars and the keyboard
        this._root.setOnApplyWindowInsetsListener((View v, WindowInsets insets) -> {
            int top = 0, bottom = 0, left = 0, right = 0;
            if (Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets bars = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.ime() | WindowInsets.Type.displayCutout());
                top = bars.top; bottom = bars.bottom; left = bars.left; right = bars.right;
            } else {
                top = insets.getSystemWindowInsetTop(); bottom = insets.getSystemWindowInsetBottom();
                left = insets.getSystemWindowInsetLeft(); right = insets.getSystemWindowInsetRight();
            }
            v.setPadding(left, top, right, bottom);
            return insets;
        });
        this.setContentView(this._root);

        if (Build.VERSION.SDK_INT >= 33) {
            this.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, this::onBack);
        }

        if (!this.handleLink(this.getIntent())) this.startOrSetup();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        this.setIntent(intent);
        this.handleLink(intent);
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        this.onBack();
    }

    private void onBack() {
        if (this._scanning) {
            this.showSetup("", this.lastHost());
            return;
        }
        if (this._web != null && this._web.canGoBack()) {
            this._web.goBack();
            return;
        }
        if (this._web == null) {
            this.finish();
            return;
        }
        new AlertDialog.Builder(this)
            .setTitle("xdesk")
            .setItems(new CharSequence[]{this.tr("Leave the application", "Quitter l'application"), this.tr("Change host", "Changer d'hôte"), this.tr("Stay", "Rester")}, (d, which) -> {
                if (which == 0) this.finish();
                else if (which == 1) this.showSetup("", this.lastHost());
            })
            .show();
    }

    @Override
    protected void onDestroy() {
        this.closeUdp();
        if (this._web != null) this._web.destroy();
        super.onDestroy();
    }

    // ---------------------------------------------------------------- //
    // Host and pinning

    private String host() {return this._prefs.getString("host", "");}

    private String pin() {return this._prefs.getString("fp", "");}

    // Last address tried (even a failed one), else the pinned host: the default of the connect screen
    private String lastHost() {
        String last = this._prefs.getString("last", "");
        return last.isEmpty() ? this.host() : last;
    }

    // Addresses tried, the newest first (at most RECENT_MAX)
    private List<String> recentHosts() {
        List<String> out = new ArrayList<>();
        for (String h: this._prefs.getString("recent", "").split("\n"))
            if (!h.isEmpty() && !out.contains(h)) out.add(h);
        if (!this.host().isEmpty() && !out.contains(this.host())) out.add(this.host());
        return out;
    }

    private void rememberHost(String origin) {
        List<String> recent = this.recentHosts();
        recent.remove(origin);
        recent.add(0, origin);
        while (recent.size() > RECENT_MAX) recent.remove(recent.size() - 1);
        this._prefs.edit().putString("last", origin).putString("recent", String.join("\n", recent)).apply();
    }

    private void forgetHost(String origin) {
        List<String> recent = this.recentHosts();
        recent.remove(origin);
        SharedPreferences.Editor edit = this._prefs.edit().putString("recent", String.join("\n", recent));
        if (origin.equals(this._prefs.getString("last", ""))) edit.remove("last");
        edit.apply();
    }

    private void startOrSetup() {
        if (this.host().isEmpty() || this.pin().isEmpty()) this.showSetup("", this.lastHost());
        else this.openWeb();
    }

    // xdesk://host:port?fp=AB:CD..&code=K7F29QXM4
    private boolean handleLink(Intent intent) {
        if (intent == null || intent.getData() == null || !"xdesk".equals(intent.getData().getScheme())) return false;
        Uri uri = intent.getData();
        String origin = normalize(uri.getHost() == null ? "" : uri.getHost() + (uri.getPort() > 0 ? ":" + uri.getPort() : ""));
        String fp = clean(uri.getQueryParameter("fp"));
        String code = uri.getQueryParameter("code");
        this._pendingCode = code == null ? "" : code;
        if (origin == null) {
            this.showSetup(this.tr("Invalid link", "Lien invalide"), this.lastHost());
            return true;
        }
        this.probe(origin, fp);
        return true;
    }

    // "host", "host:port", "https://host:port/..." -> "https://host:port" (null when invalid)
    static String normalize(String input) {
        String s = input == null ? "" : input.trim();
        if (s.isEmpty()) return null;
        if (s.startsWith("xdesk://")) s = s.substring(8);
        if (!s.contains("://")) s = "https://" + s;
        try {
            Uri uri = Uri.parse(s);
            if (!"https".equals(uri.getScheme()) || uri.getHost() == null || uri.getHost().isEmpty()) return null;
            int port = uri.getPort() > 0 ? uri.getPort() : DEFAULT_PORT;
            String h = uri.getHost();
            if (h.contains(":") && !h.startsWith("[")) h = "[" + h + "]"; // IPv6
            return "https://" + h + ":" + port;
        } catch (Exception e) {
            return null;
        }
    }

    static String clean(String fp) {
        return fp == null ? "" : fp.replaceAll("[^0-9A-Fa-f]", "").toLowerCase(Locale.ROOT);
    }

    static String pretty(String hex) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i + 1 < hex.length(); i += 2) {
            if (out.length() > 0) out.append(i % 16 == 0 ? "\n" : ":");
            out.append(hex.substring(i, i + 2).toUpperCase(Locale.ROOT));
        }
        return out.toString();
    }

    static String sha256(byte[] data) {
        try {
            byte[] d = MessageDigest.getInstance("SHA-256").digest(data);
            StringBuilder out = new StringBuilder();
            for (byte b: d) out.append(String.format(Locale.ROOT, "%02x", b & 0xff));
            return out.toString();
        } catch (Exception e) {
            return "";
        }
    }

    // Fingerprint of the certificate given to the WebView (DER from the saved state: works on every API level)
    static String fingerprintOf(SslCertificate cert) {
        if (cert == null) return "";
        try {
            if (Build.VERSION.SDK_INT >= 29 && cert.getX509Certificate() != null) return sha256(cert.getX509Certificate().getEncoded());
            Bundle b = SslCertificate.saveState(cert);
            byte[] der = b == null ? null : b.getByteArray("x509-certificate");
            return der == null ? "" : sha256(der);
        } catch (Exception e) {
            return "";
        }
    }

    // TLS context trusting ONLY the certificate whose sha256 is expected (empty expected: records it, trusts nothing else)
    private static final class PinTrust implements X509TrustManager {
        final String expected;
        String seen = "";

        PinTrust(String expected) {this.expected = expected;}

        @Override public void checkClientTrusted(X509Certificate[] chain, String auth) throws CertificateException {throw new CertificateException("client auth not used");}

        @Override public void checkServerTrusted(X509Certificate[] chain, String auth) throws CertificateException {
            if (chain == null || chain.length == 0) throw new CertificateException("no certificate");
            this.seen = sha256(chain[0].getEncoded());
            if (this.expected.isEmpty() || !MessageDigest.isEqual(this.seen.getBytes(), this.expected.getBytes())) throw new CertificateException("certificate not pinned");
        }

        @Override public X509Certificate[] getAcceptedIssuers() {return new X509Certificate[0];}
    }

    private static HttpsURLConnection pinnedConnection(String url, String fp) throws Exception {
        SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(null, new TrustManager[]{new PinTrust(fp)}, new SecureRandom());
        HttpsURLConnection c = (HttpsURLConnection) new URL(url).openConnection();
        c.setSSLSocketFactory(ctx.getSocketFactory());
        // The pin replaces the name check: the host certificate is self-signed for its LAN / tailnet addresses
        HostnameVerifier any = (h, s) -> true;
        c.setHostnameVerifier(any);
        c.setConnectTimeout(TIMEOUT);
        c.setReadTimeout(TIMEOUT);
        c.setRequestProperty("User-Agent", "xdesk-app/" + version());
        return c;
    }

    // Read the certificate of the host, then pin it (link fingerprint, or confirmation of the user)
    private void probe(String origin, String linkFp) {
        this.rememberHost(origin);
        this.showBusy(this.tr("Connecting to ", "Connexion à ") + origin);
        new Thread(() -> {
            String seen = "";
            String error = "";
            try {
                Uri uri = Uri.parse(origin);
                PinTrust trust = new PinTrust("");
                SSLContext ctx = SSLContext.getInstance("TLS");
                ctx.init(null, new TrustManager[]{trust}, new SecureRandom());
                String h = uri.getHost();
                if (h.startsWith("[")) h = h.substring(1, h.length() - 1);
                try (SSLSocket socket = (SSLSocket) ctx.getSocketFactory().createSocket()) {
                    socket.connect(new java.net.InetSocketAddress(h, uri.getPort()), TIMEOUT);
                    socket.setSoTimeout(TIMEOUT);
                    try {socket.startHandshake();} catch (Exception e) { /* refused by PinTrust on purpose: the certificate is recorded */ }
                }
                seen = trust.seen;
                if (seen.isEmpty()) error = this.tr("No TLS certificate received", "Aucun certificat TLS reçu");
            } catch (Exception e) {
                error = e.getMessage() == null ? e.toString() : e.getMessage();
            }
            final String fp = seen;
            final String err = error;
            this._ui.post(() -> {
                if (!err.isEmpty()) {
                    this.showSetup(this.tr("Host unreachable: ", "Hôte injoignable : ") + err, origin);
                    return;
                }
                if (!linkFp.isEmpty()) {
                    if (linkFp.equals(fp)) {
                        this.savePin(origin, fp);
                        return;
                    }
                    this.showSetup(this.tr("The certificate of the host does not match the link: possible interception, connection refused.", "Le certificat de l'hôte ne correspond pas au lien : interception possible, connexion refusée."), origin);
                    return;
                }
                this.confirmPin(origin, fp);
            });
        }).start();
    }

    private void confirmPin(String origin, String fp) {
        boolean changed = origin.equals(this.host()) && !this.pin().isEmpty() && !this.pin().equals(fp);
        TextView msg = new TextView(this);
        msg.setPadding(this.dp(24), this.dp(12), this.dp(24), 0);
        msg.setTextColor(changed ? WARN : TEXT);
        msg.setText((changed
            ? this.tr("WARNING: the certificate of this host CHANGED. Only accept if you regenerated it on the host.\n\n", "ATTENTION : le certificat de cet hôte a CHANGÉ. N'acceptez que si vous l'avez régénéré sur l'hôte.\n\n")
            : this.tr("Compare this fingerprint with the one shown on the host screen:\n\n", "Comparez cette empreinte avec celle affichée sur l'écran de l'hôte :\n\n")));
        TextView code = new TextView(this);
        code.setTypeface(Typeface.MONOSPACE);
        code.setTextIsSelectable(true);
        code.setTextColor(ACCENT);
        code.setText(pretty(fp));
        code.setPadding(this.dp(24), 0, this.dp(24), this.dp(8));
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.addView(msg);
        box.addView(code);
        new AlertDialog.Builder(this)
            .setTitle(this.tr("Trust this host?", "Faire confiance à cet hôte ?"))
            .setView(box)
            .setCancelable(false)
            .setPositiveButton(this.tr("Trust", "Faire confiance"), (d, w) -> this.savePin(origin, fp))
            .setNegativeButton(this.tr("Cancel", "Annuler"), (d, w) -> this.showSetup("", origin))
            .show();
    }

    private void savePin(String origin, String fp) {
        this._prefs.edit().putString("host", origin).putString("fp", fp).apply();
        this._updateChecked = false;
        this.openWeb();
    }

    // ---------------------------------------------------------------- //
    // Screens

    private int dp(int v) {return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, this.getResources().getDisplayMetrics());}

    private boolean french() {return Locale.getDefault().getLanguage().equals("fr");}

    private String tr(String en, String fr) {return this.french() ? fr : en;}

    private void clearRoot() {
        this._scanning = false;
        this.closeUdp();
        if (this._web != null) {
            this._root.removeView(this._web);
            this._web.destroy();
            this._web = null;
        }
        this._root.removeAllViews();
    }

    private void showBusy(String text) {
        this.clearRoot();
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextColor(MUTED);
        t.setGravity(Gravity.CENTER);
        t.setPadding(this.dp(24), 0, this.dp(24), 0);
        this._root.addView(t, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    private void showSetup(String error, String current) {
        this.clearRoot();
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER_VERTICAL);
        box.setPadding(this.dp(24), this.dp(24), this.dp(24), this.dp(24));

        TextView eyebrow = new TextView(this);
        eyebrow.setText(this.tr("REMOTE ACCESS", "ACCÈS À DISTANCE"));
        eyebrow.setTextColor(ACCENT);
        eyebrow.setTextSize(12);
        eyebrow.setLetterSpacing(0.1f);
        box.addView(eyebrow);

        TextView title = new TextView(this);
        title.setText(this.tr("Connect to a host", "Se connecter à un hôte"));
        title.setTextColor(TEXT);
        title.setTextSize(26);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setPadding(0, this.dp(4), 0, this.dp(8));
        box.addView(title);

        TextView lead = new TextView(this);
        lead.setText(this.tr("Address shown on the host (LAN, Tailscale, tunnel), for example 192.168.1.20:47800, or scan the QR code of the host panel (Overview, QR code).",
            "Adresse affichée sur l'hôte (LAN, Tailscale, tunnel), par exemple 192.168.1.20:47800, ou scannez le QR code du panneau de l'hôte (Vue d'ensemble, QR code)."));
        lead.setTextColor(MUTED);
        lead.setTextSize(15);
        box.addView(lead);

        EditText input = new EditText(this);
        input.setHint("https://192.168.1.20:47800");
        input.setText(current == null ? "" : current);
        input.setTextColor(TEXT);
        input.setHintTextColor(MUTED);
        input.setSingleLine(true);
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        input.setImeOptions(EditorInfo.IME_ACTION_GO);
        input.setBackgroundColor(SURFACE);
        input.setPadding(this.dp(14), this.dp(12), this.dp(14), this.dp(12));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = this.dp(18);
        box.addView(input, lp);

        TextView err = new TextView(this);
        err.setTextColor(WARN);
        err.setText(error == null ? "" : error);
        err.setPadding(0, this.dp(10), 0, 0);
        err.setVisibility(error == null || error.isEmpty() ? View.GONE : View.VISIBLE);
        box.addView(err);

        // Recent addresses: a tap connects, a long press removes it
        for (String recent: this.recentHosts()) {
            Button chip = new Button(this);
            chip.setText(recent.replaceFirst("^https://", ""));
            chip.setTextColor(recent.equals(current) ? ACCENT : TEXT);
            chip.setBackgroundColor(SURFACE);
            chip.setAllCaps(false);
            chip.setTypeface(Typeface.MONOSPACE);
            chip.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
            chip.setPadding(this.dp(14), 0, this.dp(14), 0);
            LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, this.dp(44));
            cp.topMargin = this.dp(6);
            box.addView(chip, cp);
            chip.setOnClickListener(v -> {
                input.setText(recent);
                this.connectTo(recent);
            });
            chip.setOnLongClickListener(v -> {
                this.forgetHost(recent);
                box.removeView(chip);
                return true;
            });
        }

        Button go = new Button(this);
        go.setText(this.tr("Connect", "Se connecter"));
        go.setTextColor(Color.parseColor("#07130d"));
        go.setBackgroundColor(ACCENT);
        go.setAllCaps(false);
        LinearLayout.LayoutParams gp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, this.dp(48));
        gp.topMargin = this.dp(16);
        box.addView(go, gp);

        Button scan = new Button(this);
        scan.setText(this.tr("Scan the QR code of the host", "Scanner le QR code de l'hôte"));
        scan.setTextColor(TEXT);
        scan.setBackgroundColor(SURFACE);
        scan.setAllCaps(false);
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, this.dp(48));
        sp.topMargin = this.dp(10);
        box.addView(scan, sp);
        scan.setOnClickListener(v -> this.withCamera(this::showScanner, () -> {
            err.setText(this.tr("Camera refused: type the address, or open the xdesk:// link with the camera app.", "Caméra refusée : tapez l'adresse, ou ouvrez le lien xdesk:// avec l'application appareil photo."));
            err.setVisibility(View.VISIBLE);
        }));

        Runnable submit = () -> {
            String origin = normalize(input.getText().toString());
            if (origin == null) {
                err.setText(this.tr("Invalid address", "Adresse invalide"));
                err.setVisibility(View.VISIBLE);
                return;
            }
            this.connectTo(input.getText().toString().trim());
        };
        go.setOnClickListener(v -> submit.run());
        input.setOnEditorActionListener((v, action, ev) -> {submit.run(); return true;});

        scroll.addView(box);
        this._root.addView(scroll, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    // Typed address or xdesk:// link (already checked by normalize)
    private void connectTo(String link) {
        String origin = normalize(link);
        String fp = "";

        if (origin == null) return;
        if (link.startsWith("xdesk://")) fp = clean(Uri.parse(link).getQueryParameter("fp"));
        else if (origin.equals(this.host())) fp = this.pin(); // same host: the pinned certificate must still match
        if (!fp.isEmpty() && origin.equals(this.host()) && fp.equals(this.pin())) {
            this.rememberHost(origin);
            this.openWeb();
            return;
        }
        this.probe(origin, fp);
    }

    private void openWeb() {
        this.clearRoot();
        final String origin = this.host();
        final String fp = this.pin();
        WebView web = new WebView(this);
        this._web = web;
        web.setBackgroundColor(BG);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(false);
        s.setSupportZoom(false);
        s.setUserAgentString(s.getUserAgentString() + " xdesk-app/" + version());
        web.setWebViewClient(new WebViewClient() {
            @Override
            public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
                // Self-signed host: accepted ONLY for the pinned certificate
                String seen = fingerprintOf(error.getCertificate());
                if (!seen.isEmpty() && MessageDigest.isEqual(seen.getBytes(), fp.getBytes())) {
                    handler.proceed();
                    return;
                }
                handler.cancel();
                MainActivity.this._ui.post(() -> MainActivity.this.confirmPin(origin, seen));
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                String target = uri.getScheme() + "://" + uri.getHost() + ":" + (uri.getPort() > 0 ? uri.getPort() : 443);
                if (normalize(origin).equals(normalize(target)) || target.equals(origin)) {
                    if (uri.getPath() != null && uri.getPath().startsWith("/app/xdesk.apk")) {
                        MainActivity.this.checkUpdate(true);
                        return true;
                    }
                    return false;
                }
                try {MainActivity.this.startActivity(new Intent(Intent.ACTION_VIEW, uri));} catch (Exception e) { /* no browser */ }
                return true;
            }

            @Override
            public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
                if (!request.isForMainFrame()) return;
                MainActivity.this._ui.post(() -> MainActivity.this.showSetup(MainActivity.this.tr("Host unreachable: ", "Hôte injoignable : ") + error.getDescription(), origin));
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                Uri uri = Uri.parse(url);
                String loaded = uri.getScheme() + "://" + uri.getHost() + ":" + (uri.getPort() > 0 ? uri.getPort() : 443);
                if (view == MainActivity.this._web && (normalize(origin).equals(normalize(loaded)) || loaded.equals(origin))) MainActivity.this.offerUdp(view, origin);
            }
        });
        web.setWebChromeClient(this.cameraClient(origin)); // scanner of the web client: camera for the pinned host only
        web.setDownloadListener((url, agent, disposition, mime, length) -> this.checkUpdate(true));
        this._root.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        String url = origin + "/";
        if (!this._pendingCode.isEmpty()) {
            url += "?code=" + Uri.encode(this._pendingCode);
            this._pendingCode = "";
        }
        web.loadUrl(url);
        if (!this._updateChecked) {
            this._updateChecked = true;
            this.checkUpdate(false);
        }
    }

    // ---------------------------------------------------------------- //
    // Video over udp

    private void closeUdp() {
        if (this._udp != null) this._udp.stop();
        if (this._udpPort != null) this._udpPort.close();
        this._udp = null;
        this._udpPort = null;
    }

    // A message port for the page of the pinned host only: it starts / stops the udp video with the keys of its welcome
    private void offerUdp(WebView web, String origin) {
        if (Build.VERSION.SDK_INT < 23) return;
        this.closeUdp();
        final WebMessagePort[] ports = web.createWebMessageChannel();
        final WebMessagePort mine = ports[0];
        final String host = Uri.parse(origin).getHost(); // never an address given by the page
        final UdpVideo udp = new UdpVideo(new UdpVideo.Listener() {
            @Override
            public void frame(byte[] frame) {
                final String text = "F" + Base64.encodeToString(frame, Base64.NO_WRAP);
                MainActivity.this._ui.post(() -> {if (MainActivity.this._udpPort == mine) mine.postMessage(new WebMessage(text));});
            }

            @Override
            public void active(boolean active) {
                MainActivity.this._ui.post(() -> {if (MainActivity.this._udpPort == mine) mine.postMessage(new WebMessage("S{\"active\":" + active + "}"));});
            }

            @Override
            public void gaveUp() {
                MainActivity.this._ui.post(() -> {if (MainActivity.this._udpPort == mine) mine.postMessage(new WebMessage("S{\"gaveUp\":true}"));});
            }
        });
        this._udp = udp;
        this._udpPort = mine;
        mine.setWebMessageCallback(new WebMessagePort.WebMessageCallback() {
            @Override
            public void onMessage(WebMessagePort port, WebMessage message) {
                try {
                    JSONObject m = new JSONObject(message.getData());
                    switch (m.optString("t")) {
                        case "start": {
                            JSONObject offer = m.getJSONObject("offer");
                            byte[] down = Base64.decode(offer.getString("down"), Base64.DEFAULT);
                            byte[] up = Base64.decode(offer.getString("up"), Base64.DEFAULT);
                            if (!udp.start(host, offer.getInt("port"), offer.getLong("connection"), down, up)) mine.postMessage(new WebMessage("S{\"active\":false}"));
                            break;
                        }
                        case "stop": udp.stop(); break;
                        case "rtt": udp.setRtt(m.optInt("ms", 50)); break;
                        default: break;
                    }
                } catch (Exception e) { /* malformed: ignored */ }
            }
        });
        web.postWebMessage(new WebMessage("xdesk-udp", new WebMessagePort[]{ports[1]}), Uri.parse(origin));
    }

    // ---------------------------------------------------------------- //
    // QR code scanner

    // Runtime permission of the camera, then granted / denied
    private void withCamera(Runnable granted, Runnable denied) {
        if (Build.VERSION.SDK_INT < 23 || this.checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            granted.run();
            return;
        }
        this._cameraGranted = granted;
        this._cameraDenied = denied;
        this.requestPermissions(new String[]{Manifest.permission.CAMERA}, CAMERA_REQUEST);
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        if (request != CAMERA_REQUEST) return;
        Runnable next = results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED ? this._cameraGranted : this._cameraDenied;
        this._cameraGranted = null;
        this._cameraDenied = null;
        if (next != null) next.run();
    }

    // The camera of a page: only for the given origin, and only the video
    private WebChromeClient cameraClient(String allowed) {
        return new WebChromeClient() {
            @Override
            public void onPermissionRequest(PermissionRequest request) {
                Uri asked = request.getOrigin();
                String origin = asked == null ? "" : asked.getScheme() + "://" + asked.getHost() + (asked.getPort() > 0 ? ":" + asked.getPort() : "");
                boolean video = Arrays.asList(request.getResources()).contains(PermissionRequest.RESOURCE_VIDEO_CAPTURE);
                String a = normalize(origin);
                if (!video || a == null || !a.equals(normalize(allowed))) {
                    request.deny();
                    return;
                }
                MainActivity.this._ui.post(() -> MainActivity.this.withCamera(() -> request.grant(new String[]{PermissionRequest.RESOURCE_VIDEO_CAPTURE}), request::deny));
            }
        };
    }

    // Scanner page of the APK: the assets served on a local https origin (secure context: the camera works there)
    private void showScanner() {
        this.clearRoot();
        WebView web = new WebView(this);
        this._web = web;
        this._scanning = true;
        web.setBackgroundColor(Color.BLACK);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);
        s.setAllowContentAccess(false);
        web.setWebChromeClient(this.cameraClient(SCANNER));
        web.setWebViewClient(new WebViewClient() {
            @Override
            public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                if (!SCANNER.equals(uri.getScheme() + "://" + uri.getHost())) return null;
                String path = uri.getPath() == null || uri.getPath().equals("/") ? "scan.html" : uri.getPath().substring(1);
                if (!path.equals("scan.html") && !path.equals("jsQR.js")) return new WebResourceResponse("text/plain", "utf-8", 404, "Not Found", null, null);
                try {
                    return new WebResourceResponse(path.endsWith(".js") ? "text/javascript" : "text/html", "utf-8", MainActivity.this.getAssets().open(path));
                } catch (Exception e) {
                    return null;
                }
            }

            // The page navigates to what it read: xdesk://host:port?fp=..&code=.. or https://host:port/?code=..
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                String scheme = uri.getScheme() == null ? "" : uri.getScheme();
                if (SCANNER.equals(scheme + "://" + uri.getHost())) return false;
                if (scheme.equals("xdesk-cancel")) {
                    MainActivity.this._ui.post(() -> MainActivity.this.showSetup("", MainActivity.this.lastHost()));
                    return true;
                }
                if (scheme.equals("xdesk")) {
                    MainActivity.this._ui.post(() -> MainActivity.this.handleLink(new Intent(Intent.ACTION_VIEW, uri)));
                    return true;
                }
                String origin = normalize(uri.toString());
                String code = uri.getQueryParameter("code");
                if (scheme.equals("https") && origin != null && code != null) {
                    MainActivity.this._pendingCode = code;
                    MainActivity.this._ui.post(() -> MainActivity.this.probe(origin, "")); // no fingerprint in a browser link: confirmed by the user
                    return true;
                }
                MainActivity.this._ui.post(() -> MainActivity.this.showSetup(MainActivity.this.tr("This QR code is not an xdesk link", "Ce QR code n'est pas un lien xdesk"), MainActivity.this.lastHost()));
                return true;
            }
        });
        this._root.addView(web, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        web.loadUrl(SCANNER + "/?lang=" + (this.french() ? "fr" : "en"));
    }

    // ---------------------------------------------------------------- //
    // Updates

    static String version() {
        return BuildInfo.VERSION;
    }

    // 1 when a > b, -1 when a < b ("1.2.10" > "1.2.9", suffixes like -pre ignored)
    static int compareVersions(String a, String b) {
        String[] x = a.split("[-+]")[0].split("\\.");
        String[] y = b.split("[-+]")[0].split("\\.");
        for (int i = 0; i < Math.max(x.length, y.length); ++i) {
            int u = i < x.length ? parseInt(x[i]) : 0;
            int v = i < y.length ? parseInt(y[i]) : 0;
            if (u != v) return u > v ? 1 : -1;
        }
        return 0;
    }

    static int parseInt(String s) {
        try {return Integer.parseInt(s.replaceAll("[^0-9]", ""));} catch (Exception e) {return 0;}
    }

    private void checkUpdate(boolean asked) {
        final String origin = this.host();
        final String fp = this.pin();
        new Thread(() -> {
            String remote = "";
            try {
                HttpsURLConnection c = pinnedConnection(origin + "/app/version", fp);
                if (c.getResponseCode() == 200) {
                    try (InputStream in = c.getInputStream()) {
                        remote = new String(readAll(in, 64), "UTF-8").trim();
                    }
                }
                c.disconnect();
            } catch (Exception e) {
                remote = "";
            }
            final String latest = remote;
            this._ui.post(() -> {
                if (latest.isEmpty()) {
                    if (asked) Toast.makeText(this, this.tr("No update available on the host", "Aucune mise à jour sur l'hôte"), Toast.LENGTH_SHORT).show();
                    return;
                }
                if (compareVersions(latest, version()) <= 0) {
                    if (asked) Toast.makeText(this, this.tr("xdesk is up to date (", "xdesk est à jour (") + version() + ")", Toast.LENGTH_SHORT).show();
                    return;
                }
                new AlertDialog.Builder(this)
                    .setTitle(this.tr("Update available", "Mise à jour disponible"))
                    .setMessage(version() + " -> " + latest + "\n\n" + this.tr("The update comes from the host (pinned certificate).", "La mise à jour vient de l'hôte (certificat épinglé)."))
                    .setPositiveButton(this.tr("Update", "Mettre à jour"), (d, w) -> this.downloadAndInstall(origin, fp))
                    .setNegativeButton(this.tr("Later", "Plus tard"), null)
                    .show();
            });
        }).start();
    }

    private void downloadAndInstall(String origin, String fp) {
        Toast.makeText(this, this.tr("Downloading the update...", "Téléchargement de la mise à jour..."), Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            String error = "";
            try {
                File apk = new File(this.getCacheDir(), "update.apk");
                HttpsURLConnection c = pinnedConnection(origin + "/app/xdesk.apk", fp);
                c.setReadTimeout(60000);
                if (c.getResponseCode() != 200) throw new Exception("HTTP " + c.getResponseCode());
                try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(apk)) {
                    byte[] buffer = new byte[65536];
                    for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
                }
                c.disconnect();
                this.install(apk);
            } catch (Exception e) {
                error = e.getMessage() == null ? e.toString() : e.getMessage();
            }
            final String err = error;
            if (!err.isEmpty()) this._ui.post(() -> Toast.makeText(this, this.tr("Update failed: ", "Échec de la mise à jour : ") + err, Toast.LENGTH_LONG).show());
        }).start();
    }

    // PackageInstaller session: the system asks the user (InstallReceiver opens its confirmation)
    private void install(File apk) throws Exception {
        PackageInstaller installer = this.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setAppPackageName(this.getPackageName());
        int id = installer.createSession(params);
        try (PackageInstaller.Session session = installer.openSession(id)) {
            try (InputStream in = new FileInputStream(apk); OutputStream out = session.openWrite("xdesk.apk", 0, apk.length())) {
                byte[] buffer = new byte[65536];
                for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
                session.fsync(out);
            }
            Intent intent = new Intent(this, InstallReceiver.class);
            int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
            PendingIntent pending = PendingIntent.getBroadcast(this, id, intent, flags);
            session.commit(pending.getIntentSender());
        }
    }

    static byte[] readAll(InputStream in, int max) throws Exception {
        byte[] out = new byte[max];
        int size = 0;
        for (int n; size < max && (n = in.read(out, size, max - size)) > 0;) size += n;
        byte[] r = new byte[size];
        System.arraycopy(out, 0, r, 0, size);
        return r;
    }

    // Installed version (from the manifest written by scripts/build_apk.sh)
    static final class BuildInfo {
        static String VERSION = "0";
    }

}
