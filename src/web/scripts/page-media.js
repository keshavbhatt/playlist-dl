// name:     page-media
// purpose:  tell the Browser page what media the current document plays
//           (FEATURES B4): the largest <video> or an <audio> element, its
//           height and, when the element streams a plain http(s) file, that
//           URL. The page shows "Download detected: 1080p video" from it.
// depends:  standard DOM only (<video>, <audio>, MutationObserver, Open Graph
//           meta tags); the QWebChannel `bridge.pageMedia(state)` slot. On
//           YouTube the channel bootstrap.js opened is reused through
//           window.__red.onBridge; elsewhere this script opens its own.
// verified: 2026-09-19 against www.youtube.com, vimeo.com and a plain <video>
// on-fail:  no-op; the toolbar's own Download this button still works
(function () {
    'use strict';
    if (window.__pldlPageMedia) {
        return;
    }
    window.__pldlPageMedia = true;
    // Google's sign-in pages inspect their environment; leave them alone.
    var host = location.hostname || '';
    if (/(^|\.)accounts\.(google|youtube)\.com$/.test(host)) {
        return;
    }
    if (window.top !== window) {
        return; // the main frame reports; scripts do not run on sub frames anyway
    }

    var bridge = null;
    var lastSent = '';
    var timer = 0;

    function send(state) {
        var key = JSON.stringify(state);
        if (key === lastSent || !bridge) {
            return;
        }
        lastSent = key;
        try {
            bridge.pageMedia(state);
        } catch (e) {
            // the channel went away with the page; nothing to do
        }
    }

    function directUrl(el) {
        var src = el.currentSrc || el.src || '';
        if (!src) {
            var source = el.querySelector && el.querySelector('source[src]');
            src = source ? source.src : '';
        }
        return /^https?:/i.test(src) ? src : '';
    }

    function scan() {
        timer = 0;
        var best = null;
        var bestArea = -1;
        var videos = document.querySelectorAll('video');
        for (var i = 0; i < videos.length; i++) {
            var v = videos[i];
            var has = v.readyState > 0 || v.currentSrc || v.src || v.querySelector('source[src]');
            if (!has) {
                continue;
            }
            var area = (v.videoWidth || 0) * (v.videoHeight || 0);
            if (area > bestArea) {
                bestArea = area;
                best = v;
            }
        }
        if (best) {
            send({ kind: 'video', height: best.videoHeight || 0, direct: directUrl(best), title: document.title || '' });
            return;
        }
        var audios = document.querySelectorAll('audio');
        for (var j = 0; j < audios.length; j++) {
            var a = audios[j];
            if (a.readyState > 0 || a.currentSrc || a.src || a.querySelector('source[src]')) {
                send({ kind: 'audio', height: 0, direct: directUrl(a), title: document.title || '' });
                return;
            }
        }
        // No element yet (a click-to-play poster): the page says it is a video.
        var og = document.querySelector('meta[property="og:type"]');
        if (og && /^video\b/i.test(og.getAttribute('content') || '')) {
            send({ kind: 'video', height: 0, direct: '', title: document.title || '' });
            return;
        }
        send({ kind: '', height: 0, direct: '', title: '' });
    }

    function schedule() {
        if (!timer) {
            timer = setTimeout(scan, 400);
        }
    }

    function start(b) {
        bridge = b;
        scan();
        // Players are created after load and change quality while playing.
        ['loadedmetadata', 'playing', 'resize', 'emptied'].forEach(function (name) {
            document.addEventListener(name, schedule, true);
        });
        var root = document.body || document.documentElement;
        if (root) {
            new MutationObserver(schedule).observe(root, { childList: true, subtree: true });
        }
        // Single-page sites (YouTube) swap the document without a load.
        window.addEventListener('yt-navigate-finish', schedule);
        window.addEventListener('popstate', schedule);
    }

    try {
        if (window.__red && typeof window.__red.onBridge === 'function') {
            window.__red.onBridge(start);
        } else if (typeof QWebChannel === 'function' && window.qt && window.qt.webChannelTransport) {
            new QWebChannel(window.qt.webChannelTransport, function (channel) {
                if (channel.objects.bridge) {
                    start(channel.objects.bridge);
                }
            });
        }
    } catch (e) {
        // no channel: the page keeps working, only the detection is off
    }
})();
