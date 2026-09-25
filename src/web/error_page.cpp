#include "web/error_page.h"

using namespace Qt::StringLiterals;

namespace pldl::web {

QString errorPageHtml(const ErrorPageStyle& style, const QString& title, const QString& detail,
                      const QUrl& retryUrl)
{
    const QColor ring = style.accent; // drawn at 16 percent alpha below
    QString html = uR"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Playlist Downloader</title>
<style>
  html,body{height:100%;margin:0}
  body{background:{{BG}};color:{{FG}};
    font-family:'Segoe UI',Ubuntu,'Helvetica Neue',Helvetica,Arial,sans-serif;
    display:flex;align-items:center;justify-content:center;text-align:center;
    -webkit-user-select:none;user-select:none}
  .wrap{max-width:420px;padding:32px 24px}
  .mark{width:96px;height:96px;border-radius:24px;margin:0 auto 26px;
    background:{{ACCENT}};box-shadow:0 0 0 12px {{RING}};
    display:flex;align-items:center;justify-content:center}
  .mark svg{width:52px;height:52px;fill:none;stroke:#fff;stroke-width:6;
    stroke-linecap:round;stroke-linejoin:round}
  h1{font-size:22px;font-weight:600;margin:0 0 10px}
  p{font-size:15px;line-height:1.5;color:{{MUTED}};margin:0 0 26px}
  a.button{display:inline-block;text-decoration:none;cursor:pointer;
    background:{{ACCENT}};color:#fff;font-size:15px;font-weight:600;
    padding:12px 30px;border-radius:24px;transition:background .15s ease}
  a.button:hover{background:{{ACCENTHOVER}}}
  a.button:focus-visible{outline:2px solid {{FG}};outline-offset:3px}
  a.button:active{transform:translateY(1px)}
</style></head>
<body>
  <div class="wrap">
    <div class="mark">
      <svg viewBox="0 0 64 64" aria-hidden="true">
        <circle cx="32" cy="32" r="22"/>
        <path d="M14 20 Q32 30 50 20 M14 44 Q32 34 50 44 M32 10 V54 M10 32 H54"/>
      </svg>
    </div>
    <h1>{{TITLE}}</h1>
    <p>{{DETAIL}}</p>
    <a class="button" href="{{RETRY}}">Try again</a>
  </div>
</body></html>)HTML"_s;

    html.replace(u"{{BG}}"_s, style.background.name());
    html.replace(u"{{FG}}"_s, style.text.name());
    html.replace(u"{{MUTED}}"_s, style.muted.name());
    html.replace(u"{{ACCENT}}"_s, style.accent.name());
    html.replace(u"{{ACCENTHOVER}}"_s, style.accentHover.name());
    html.replace(u"{{RING}}"_s, u"rgba(%1,%2,%3,0.16)"_s.arg(ring.red()).arg(ring.green()).arg(ring.blue()));
    html.replace(u"{{TITLE}}"_s, title.toHtmlEscaped());
    html.replace(u"{{DETAIL}}"_s, detail.toHtmlEscaped());
    html.replace(u"{{RETRY}}"_s, QString::fromUtf8(retryUrl.toEncoded()).toHtmlEscaped());
    return html;
}

QString startPageHtml(const ErrorPageStyle& style, const QString& title, const QString& detail)
{
    const QColor ring = style.accent;
    QString html = uR"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title></title>
<style>
  html,body{height:100%;margin:0}
  body{background:{{BG}};color:{{FG}};
    font-family:'Segoe UI',Ubuntu,'Helvetica Neue',Helvetica,Arial,sans-serif;
    display:flex;align-items:center;justify-content:center;text-align:center;
    -webkit-user-select:none;user-select:none}
  .wrap{max-width:460px;padding:32px 24px}
  .mark{width:88px;height:88px;border-radius:22px;margin:0 auto 24px;
    background:{{ACCENT}};box-shadow:0 0 0 12px {{RING}};
    display:flex;align-items:center;justify-content:center}
  .mark svg{width:48px;height:48px;fill:none;stroke:#fff;stroke-width:6;
    stroke-linecap:round;stroke-linejoin:round}
  h1{font-size:20px;font-weight:600;margin:0 0 10px}
  p{font-size:14px;line-height:1.55;color:{{MUTED}};margin:0}
</style></head>
<body>
  <div class="wrap">
    <div class="mark">
      <svg viewBox="0 0 64 64" aria-hidden="true">
        <path d="M14 18 H50 M14 32 H38 M14 46 H30"/>
        <path d="M46 34 V54 M38 46 L46 54 L54 46"/>
      </svg>
    </div>
    <h1>{{TITLE}}</h1>
    <p>{{DETAIL}}</p>
  </div>
</body></html>)HTML"_s;
    html.replace(u"{{BG}}"_s, style.background.name());
    html.replace(u"{{FG}}"_s, style.text.name());
    html.replace(u"{{MUTED}}"_s, style.muted.name());
    html.replace(u"{{ACCENT}}"_s, style.accent.name());
    html.replace(u"{{RING}}"_s, u"rgba(%1,%2,%3,0.16)"_s.arg(ring.red()).arg(ring.green()).arg(ring.blue()));
    html.replace(u"{{TITLE}}"_s, title.toHtmlEscaped());
    html.replace(u"{{DETAIL}}"_s, detail.toHtmlEscaped());
    return html;
}

} // namespace pldl::web
