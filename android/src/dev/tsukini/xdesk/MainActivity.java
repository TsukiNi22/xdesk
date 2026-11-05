/*
 * xdesk android application: a WebView of the web client served by the host, with:
 *  - the host address asked once (or taken from a xdesk://host:port?fp=..&code=.. link)
 *  - certificate pinning: the sha256 of the host certificate is shown and confirmed at the first connection (or checked
 *    against the fp of the link), then every TLS error is accepted ONLY for that exact certificate
 *  - automatic updates: /app/version of the host compared with the installed version, /app/xdesk.apk installed with
 *    the PackageInstaller (user confirmation of the system)
*/
package dev.tsukini.xdesk;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.Intent;
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
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.webkit.SslErrorHandler;
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
import java.util.Locale;

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
                else if (which == 1) this.showSetup("", this.host());
            })
            .show();
    }

    @Override
    protected void onDestroy() {
        if (this._web != null) this._web.destroy();
        super.onDestroy();
    }

    // ---------------------------------------------------------------- //
    // Host and pinning

    private String host() {return this._prefs.getString("host", "");}

    private String pin() {return this._prefs.getString("fp", "");}

    private void startOrSetup() {
        if (this.host().isEmpty() || this.pin().isEmpty()) this.showSetup("", this.host());
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
            this.showSetup(this.tr("Invalid link", "Lien invalide"), "");
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
        lead.setText(this.tr("Address shown on the host (LAN, Tailscale, tunnel), for example 192.168.1.20:47800, or open the xdesk:// link of the host.",
            "Adresse affichée sur l'hôte (LAN, Tailscale, tunnel), par exemple 192.168.1.20:47800, ou ouvrez le lien xdesk:// de l'hôte."));
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

        Button go = new Button(this);
        go.setText(this.tr("Connect", "Se connecter"));
        go.setTextColor(Color.parseColor("#07130d"));
        go.setBackgroundColor(ACCENT);
        go.setAllCaps(false);
        LinearLayout.LayoutParams gp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, this.dp(48));
        gp.topMargin = this.dp(16);
        box.addView(go, gp);

        Runnable submit = () -> {
            String origin = normalize(input.getText().toString());
            if (origin == null) {
                err.setText(this.tr("Invalid address", "Adresse invalide"));
                err.setVisibility(View.VISIBLE);
                return;
            }
            String link = input.getText().toString().trim();
            String fp = "";
            if (link.startsWith("xdesk://")) fp = clean(Uri.parse(link).getQueryParameter("fp"));
            else if (origin.equals(this.host())) fp = this.pin(); // same host: the pinned certificate must still match
            if (!fp.isEmpty() && origin.equals(this.host()) && fp.equals(this.pin())) {
                this.openWeb();
                return;
            }
            this.probe(origin, fp);
        };
        go.setOnClickListener(v -> submit.run());
        input.setOnEditorActionListener((v, action, ev) -> {submit.run(); return true;});

        scroll.addView(box);
        this._root.addView(scroll, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
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
        });
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
