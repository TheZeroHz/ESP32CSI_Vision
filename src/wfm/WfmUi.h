#pragma once
// Embedded Explorer UI for WebFileManager (ASCII-only PROGMEM HTML).

#include <Arduino.h>

static const char WFM_INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Web File Manager - File Explorer</title>
<style>
:root{
  --bg:#f3f3f3;--pane:#ffffff;--ink:#1a1a1a;--muted:#605e5c;--line:#edebe9;
  --line2:#d2d0ce;--sel:#cce4f7;--selb:#90c6f0;--acc:#0078d4;--acc2:#106ebe;
  --danger:#c50f1f;--hover:#f5f5f5;--folder:#ffb900;--shadow:0 4px 16px rgba(0,0,0,.14);
  --font:"Segoe UI",Tahoma,Geneva,Verdana,sans-serif;--mono:Consolas,"Courier New",monospace;
}
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:var(--bg);color:var(--ink);font:13px/1.35 var(--font)}
button,input{font:inherit}
.app{display:flex;flex-direction:column;height:100vh;min-height:540px}
.titlebar{display:flex;align-items:center;gap:10px;height:36px;padding:0 12px;background:#fff;border-bottom:1px solid var(--line)}
.appico{width:18px;height:18px;border-radius:4px;background:var(--acc);color:#fff;display:grid;place-items:center}
.appico svg{width:12px;height:12px;fill:#fff}
.titlebar h1{margin:0;font-size:12px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.titlebar .spacer{flex:1}
.gh-link{display:inline-flex;align-items:center;gap:6px;color:var(--ink);text-decoration:none;font-size:11px;font-weight:600;padding:3px 8px;border-radius:4px;border:1px solid transparent}
.gh-link:hover{background:var(--hover);border-color:var(--line2);color:#24292f}
.gh-link svg{width:14px;height:14px;fill:currentColor;flex:0 0 auto}
.statuspill{color:var(--muted);font-size:11px;white-space:nowrap}
.ribbon{display:flex;flex-wrap:wrap;gap:2px 0;align-items:center;min-height:40px;padding:4px 8px;background:#fff;border-bottom:1px solid var(--line)}
.ribbon .group{display:flex;gap:2px;align-items:center;padding:0 8px;border-right:1px solid var(--line)}
.ribbon .group:last-child{border-right:0}
.tbtn{border:1px solid transparent;background:transparent;border-radius:4px;padding:6px 10px;cursor:pointer;color:var(--ink)}
.tbtn:hover{background:var(--hover);border-color:var(--line2)}
.tbtn:disabled{opacity:.38;cursor:default}
.tbtn:disabled:hover{background:transparent;border-color:transparent}
.tbtn.primary{background:var(--acc);color:#fff;border-color:var(--acc)}
.tbtn.primary:hover{background:var(--acc2)}
.tbtn.danger{color:var(--danger)}
.navrow{display:grid;grid-template-columns:72px minmax(0,1fr) 260px;gap:8px;align-items:center;height:40px;padding:0 10px;background:#fff;border-bottom:1px solid var(--line)}
.navbtns{display:flex;gap:2px}
.addr,.search{display:flex;align-items:center;height:30px;border:1px solid var(--line2);border-radius:4px;background:#fff;overflow:hidden}
.addr:focus-within,.search:focus-within{border-color:var(--acc);box-shadow:0 0 0 1px var(--acc)}
.crumbwrap{display:flex;align-items:center;flex:1;overflow:auto;padding:0 4px;scrollbar-width:none}
.crumbwrap::-webkit-scrollbar{display:none}
.addr a,.addr .here{color:var(--ink);text-decoration:none;padding:4px 6px;border-radius:3px;white-space:nowrap}
.addr a:hover{background:var(--hover)}
.addr .sep{color:var(--muted);padding:0 2px}
.search input{flex:1;border:0;outline:none;padding:0 10px;min-width:0;height:100%;background:transparent}
.search button{border:0;background:transparent;height:100%;padding:0 12px;cursor:pointer;color:var(--acc);font-weight:600}
.body{display:grid;grid-template-columns:210px minmax(0,1fr) 300px;flex:1;min-height:0}
@media(max-width:1000px){.body{grid-template-columns:minmax(0,1fr)}.sidebar,.inspector{display:none}}
.sidebar{background:#faf9f8;border-right:1px solid var(--line);overflow:auto;padding:6px 0}
.side-label{padding:8px 14px 4px;font-size:11px;color:var(--muted);font-weight:600}
.side-item{display:flex;gap:8px;align-items:center;margin:1px 6px;padding:6px 10px;border-radius:4px;cursor:pointer;border:1px solid transparent}
.side-item:hover{background:#f0f0f0}
.side-item.on{background:var(--sel);border-color:var(--selb)}
.side-item .si{width:16px;height:16px;flex:0 0 auto;color:var(--acc)}
.side-item .si svg{width:16px;height:16px;display:block}
.side-item .si.drive svg{fill:var(--acc)}
.side-item .si.folder svg{fill:var(--folder);stroke:#8a6914;stroke-width:.5}
.main{display:flex;flex-direction:column;min-width:0;background:var(--pane);min-height:0;position:relative}
.cols{display:grid;grid-template-columns:28px minmax(140px,1fr) 120px 100px;padding:0 4px;border-bottom:1px solid var(--line);background:#faf9f8;color:var(--muted);font-size:12px;font-weight:600}
.cols span{padding:8px;border-right:1px solid var(--line)}
.cols span:last-child{border-right:0}
.list{flex:1;overflow:auto;user-select:none}
.row{display:grid;grid-template-columns:28px minmax(140px,1fr) 120px 100px;align-items:center;padding:0 4px;min-height:28px;border:1px solid transparent;cursor:default}
.row:hover{background:var(--hover)}
.row.on{background:var(--sel);border-color:var(--selb)}
.row.marked{background:#e8f4ff;border-color:#90c6f0}
.row .chk{display:flex;align-items:center;justify-content:center;padding:0}
.row .chk input{width:14px;height:14px;margin:0;cursor:pointer}
.upload-panel{display:none;padding:8px 12px;border-bottom:1px solid var(--line);background:#fff}
.upload-panel.on{display:block}
.upload-panel .urow{display:flex;justify-content:space-between;gap:10px;font-size:12px;margin-bottom:6px}
.upload-panel .ubar{height:8px;border-radius:99px;background:#edebe9;overflow:hidden}
.upload-panel .ubar i{display:block;height:100%;width:0;background:var(--acc);transition:width .15s linear}
.main.drop-target{outline:2px dashed var(--acc);outline-offset:-4px;background:rgba(0,120,212,.06)}
.drop-banner{display:none;position:absolute;inset:8px;z-index:5;align-items:center;justify-content:center;pointer-events:none;border:2px dashed var(--acc);border-radius:8px;background:rgba(255,255,255,.92);color:var(--acc);font-weight:600;font-size:14px}
.main.drop-target .drop-banner{display:flex}
.selbar{display:none;align-items:center;gap:8px;padding:6px 10px;background:#e8f4ff;border-bottom:1px solid #90c6f0;font-size:12px}
.selbar.on{display:flex}
.row .c{padding:3px 8px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.row .name{display:flex;gap:8px;align-items:center;min-width:0}
.row .name span:last-child{overflow:hidden;text-overflow:ellipsis}
.fi{width:16px;height:16px;flex:0 0 auto}
.fi svg{width:16px;height:16px;display:block}
.fi.dir svg{fill:var(--folder);stroke:#8a6914;stroke-width:.4}
.fi.img svg{fill:#13a10e}
.fi.vid svg{fill:#0078d4}
.fi.aud svg{fill:#8764b8}
.fi.txt svg{fill:#605e5c}
.fi.file svg{fill:#8764b8}
.dim{color:var(--muted)}
.statusbar{display:flex;justify-content:space-between;gap:10px;padding:5px 10px;background:#fff;border-top:1px solid var(--line);font-size:12px;color:var(--muted)}
.inspector{background:#fff;border-left:1px solid var(--line);display:flex;flex-direction:column;min-height:0}
.insp-h{padding:10px 12px;border-bottom:1px solid var(--line)}
.insp-h h2{margin:0;font-size:13px;font-weight:600;line-height:1.3;overflow-wrap:anywhere;word-break:break-word}
.insp-h p{margin:4px 0 0;color:var(--muted);font-size:11px;line-height:1.35;overflow-wrap:anywhere;word-break:break-word}
.insp-b{flex:1;overflow:auto;padding:10px;display:flex;flex-direction:column;gap:10px}
.stage-wrap{position:relative;flex:1;min-height:200px;display:flex;flex-direction:column}
.stage{
  flex:1;min-height:200px;border:1px solid var(--line);border-radius:4px;background:#111;
  display:flex;align-items:center;justify-content:center;overflow:auto;padding:8px;
}
.stage.light{background:#faf9f8}
.stage img,.stage video{max-width:100%;max-height:min(46vh,420px);object-fit:contain;display:block}
.stage audio{width:100%}
.stage pre{margin:0;width:100%;max-height:min(46vh,420px);overflow:auto;padding:10px;background:#1e1e1e;color:#d4d4d4;font:12px/1.45 var(--mono);white-space:pre-wrap;word-break:break-word;border-radius:4px}
.stage textarea.txt-edit{margin:0;width:100%;min-height:min(40vh,360px);max-height:min(50vh,480px);resize:vertical;padding:10px;background:#1e1e1e;color:#d4d4d4;font:12px/1.45 var(--mono);border:1px solid #333;border-radius:4px;box-sizing:border-box}
.stage .hint{color:var(--muted);text-align:center;padding:16px;max-width:28ch}
.stage .play-ready{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px;padding:20px;text-align:center;color:var(--muted);width:100%}
.stage .play-ready .tbtn.primary{min-width:96px}
.preview-bar{display:none;gap:8px;align-items:center;margin-top:8px;flex-wrap:wrap}
.preview-bar.on{display:flex}
.preview-bar .tbtn.primary{background:var(--acc);color:#fff;border-color:var(--acc)}
.modal-b textarea.txt-edit{width:100%;min-height:50vh;resize:vertical;padding:10px;background:#1e1e1e;color:#d4d4d4;font:12px/1.45 var(--mono);border:1px solid #333;border-radius:4px;box-sizing:border-box}
.details{font:12px/1.45 var(--mono)}
.details dt{color:var(--muted);margin-top:8px}
.details dd{margin:2px 0 0;overflow-wrap:anywhere;word-break:break-word}
.meter{padding:10px 12px;border-top:1px solid var(--line);font-size:11px;color:var(--muted)}
.bar{height:6px;border-radius:99px;background:#edebe9;overflow:hidden;margin-top:6px}
.bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .35s ease}
.bar.scan i{width:28%!important;background:linear-gradient(90deg,#ddd,var(--acc),#ddd);background-size:200% 100%;animation:sh 1s linear infinite}
.empty,.loading{padding:36px 16px;text-align:center;color:var(--muted)}
.spin{width:20px;height:20px;margin:0 auto 8px;border-radius:50%;border:2px solid #ddd;border-top-color:var(--acc);animation:sp .7s linear infinite}
.menu{position:fixed;z-index:100;min-width:200px;padding:4px;background:#fff;border:1px solid var(--line2);box-shadow:var(--shadow);border-radius:6px;display:none}
.menu.open{display:block}
.menu button{display:block;width:100%;text-align:left;border:0;background:transparent;padding:7px 12px;border-radius:4px;cursor:pointer}
.menu button:hover{background:var(--sel)}
.menu button.danger{color:var(--danger)}
.menu button:disabled{opacity:.35;cursor:default}
.menu .sep{height:1px;margin:4px 6px;background:var(--line)}
.fi.vol-sd{background:#0078d4}
.fi.vol-flash{background:#107c10}
.fi.vol{background:#8764b8}
.fi.vol-sd svg,.fi.vol-flash svg,.fi.vol svg{fill:#fff}
.si.vol-sd,.si.vol-flash,.si.vol{background:transparent}
.si.vol-sd svg{fill:#0078d4}
.si.vol-flash svg{fill:#107c10}
.si.vol svg{fill:#8764b8}
.login-bg{position:fixed;inset:0;z-index:300;background:rgba(0,0,0,.45);display:none;align-items:center;justify-content:center}
.login-bg.open{display:flex}
.login-box{width:min(360px,92vw);background:#fff;border-radius:10px;padding:22px 20px;box-shadow:var(--shadow)}
.login-box h2{margin:0 0 6px;font-size:16px}
.login-box p{margin:0 0 14px;color:var(--muted);font-size:12px}
.login-box label{display:block;font-size:12px;margin:10px 0 4px;color:var(--muted)}
.login-box input{width:100%;padding:8px 10px;border:1px solid var(--line2);border-radius:4px}
.login-box .err{color:var(--danger);font-size:12px;min-height:16px;margin-top:8px}
.login-box .rowbtns{display:flex;gap:8px;margin-top:14px}
.dlg-bg{position:fixed;inset:0;z-index:280;background:rgba(0,0,0,.4);display:none;align-items:center;justify-content:center;padding:16px}
.dlg-bg.open{display:flex}
.dlg{width:min(420px,94vw);background:#fff;border-radius:10px;box-shadow:0 16px 48px rgba(0,0,0,.28);overflow:hidden;border:1px solid var(--line2)}
.dlg-h{padding:14px 16px 0}
.dlg-h h2{margin:0;font-size:15px;font-weight:600}
.dlg-b{padding:10px 16px 4px}
.dlg-b p{margin:0;font-size:13px;line-height:1.45;color:var(--ink);white-space:pre-wrap;word-break:break-word}
.dlg-b .dim{color:var(--muted);font-size:12px;margin-top:6px}
.dlg-b label{display:block;font-size:12px;color:var(--muted);margin:8px 0 4px}
.dlg-b input{width:100%;padding:8px 10px;border:1px solid var(--line2);border-radius:4px;font:inherit}
.dlg-f{display:flex;justify-content:flex-end;gap:8px;padding:12px 16px 14px;background:#faf9f8;border-top:1px solid var(--line)}
.dlg-f .tbtn.primary{background:var(--acc);color:#fff;border-color:var(--acc)}
.dlg-f .tbtn.primary:hover{background:var(--acc2)}
.dlg-f .tbtn.danger-fill{background:var(--danger);color:#fff;border-color:var(--danger)}
.toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);z-index:320;background:#323130;color:#fff;padding:10px 16px;border-radius:8px;font-size:12px;display:none;box-shadow:0 8px 24px rgba(0,0,0,.25);max-width:min(480px,92vw);text-align:center}
.modal-bg{
  position:fixed;inset:0;z-index:200;background:rgba(0,0,0,.55);display:none;
  align-items:center;justify-content:center;padding:18px;
}
.modal-bg.open{display:flex}
.modal{
  width:min(1100px,96vw);height:min(860px,92vh);background:#fff;border-radius:10px;
  box-shadow:0 20px 60px rgba(0,0,0,.35);display:flex;flex-direction:column;overflow:hidden;
}
.modal-h{display:flex;align-items:center;gap:10px;padding:10px 14px;border-bottom:1px solid var(--line);background:#faf9f8}
.modal-h h3{margin:0;flex:1;font-size:14px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.modal-b{flex:1;min-height:0;background:#111;display:flex;align-items:center;justify-content:center;position:relative}
.modal-b.light{background:#f3f3f3}
.modal-b img,.modal-b video{max-width:100%;max-height:100%;object-fit:contain}
.modal-b audio{width:min(90%,640px)}
.modal-b pre{margin:0;width:100%;height:100%;overflow:auto;padding:16px;color:#d4d4d4;font:13px/1.45 var(--mono);white-space:pre-wrap;word-break:break-word}
.modal-b .hint{color:#bbb;text-align:center;padding:24px;max-width:36ch}
.modal-f{display:flex;gap:8px;align-items:center;padding:10px 14px;border-top:1px solid var(--line);background:#fff}
.modal-f .spacer{flex:1}
@keyframes sp{to{transform:rotate(360deg)}}
@keyframes sh{to{background-position:-200% 0}}
</style>
</head>
<body>
<div class="app">
  <div class="titlebar">
    <div class="appico"><svg viewBox="0 0 16 16"><path d="M2 3h5l1 1h6v9H2z"/></svg></div>
    <h1 id="winTitle">Web File Manager</h1>
    <div class="spacer"></div>
    <a id="homeLink" class="gh-link" href="#" style="display:none" title="Back to camera UI">Camera</a>
    <a class="gh-link" href="https://github.com/TheZeroHz" target="_blank" rel="noopener noreferrer" title="GitHub - TheZeroHz">
      <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82A7.6 7.6 0 0 1 8 3.58c.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.01 8.01 0 0 0 16 8c0-4.42-3.58-8-8-8z"/></svg>
      <span>TheZeroHz</span>
    </a>
    <div class="statuspill" id="linkMeta">Connecting...</div>
  </div>

  <div class="ribbon">
    <div class="group">
      <button class="tbtn" type="button" id="btnBack" disabled>Back</button>
      <button class="tbtn" type="button" id="btnUp">Up</button>
      <button class="tbtn" type="button" id="btnRefresh">Refresh</button>
    </div>
    <div class="group">
      <button class="tbtn" type="button" id="btnNewFolder">New folder</button>
      <button class="tbtn" type="button" id="btnNewFile">New file</button>
      <button class="tbtn" type="button" id="btnSelectAll">Select all</button>
      <button class="tbtn" type="button" id="btnClearSel" disabled>Clear marks</button>
      <button class="tbtn" type="button" id="btnCut" disabled>Cut</button>
      <button class="tbtn" type="button" id="btnCopy" disabled>Copy</button>
      <button class="tbtn" type="button" id="btnPaste" disabled>Paste</button>
      <button class="tbtn" type="button" id="btnRename" disabled>Rename</button>
      <button class="tbtn danger" type="button" id="btnDelete" disabled>Delete</button>
    </div>
    <div class="group">
      <button class="tbtn primary" type="button" id="btnPreview" disabled>Preview</button>
      <button class="tbtn" type="button" id="btnDownload" disabled>Download</button>
      <button class="tbtn" type="button" id="btnMoveMarked" disabled>Move marked...</button>
      <button class="tbtn" type="button" id="btnUpload" title="Select one or more files (Ctrl/Shift click in the dialog)">Upload files</button>
      <button class="tbtn" type="button" id="btnUploadFolder" title="Upload a whole folder (keeps subfolders)">Upload folder</button>
    </div>
  </div>

  <div class="navrow">
    <div class="navbtns"><button class="tbtn" type="button" id="btnHome">Home</button></div>
    <div class="addr"><div class="crumbwrap" id="crumbs"></div></div>
    <form class="search" id="searchForm" autocomplete="off">
      <input id="searchBox" type="search" placeholder="Search Web File Manager"/>
      <button type="submit">Search</button>
    </form>
  </div>

  <div class="body">
    <aside class="sidebar">
      <div class="side-label">This SD</div>
      <div class="side-item on" data-path="/" id="sideRoot">
        <span class="si drive"><svg viewBox="0 0 16 16"><rect x="1" y="4" width="14" height="9" rx="1"/><path d="M4 11h2" stroke="#fff" stroke-width="1.5"/></svg></span>
        <span>Web File Manager (C:)</span>
      </div>
      <div class="side-label">Quick access</div>
      <div id="quick"></div>
    </aside>

    <section class="main" id="mainPane">
      <div class="drop-banner" id="dropBanner">Drop files here to upload</div>
      <div class="upload-panel" id="uploadPanel">
        <div class="urow"><span id="uploadLabel">Transferring...</span><span id="uploadPct">0%</span></div>
        <div class="urow" style="margin-bottom:4px"><span class="dim" id="uploadSpeed">--</span><span class="dim" id="uploadEta"></span></div>
        <div class="ubar"><i id="uploadBar"></i></div>
      </div>
      <div class="selbar" id="selBar">
        <span id="markMeta">0 marked</span>
        <button class="tbtn" type="button" id="btnMarkCopy">Copy marked</button>
        <button class="tbtn" type="button" id="btnMarkCut">Cut marked</button>
        <button class="tbtn" type="button" id="btnMarkDownload">Download marked</button>
        <button class="tbtn danger" type="button" id="btnMarkDelete">Delete marked</button>
      </div>
      <div class="cols"><span><input type="checkbox" id="chkAll" title="Select all"/></span><span>Name</span><span>Type</span><span>Size</span></div>
      <div class="list" id="list"></div>
      <div class="statusbar">
        <span id="countMeta">0 items</span>
        <span id="selMeta">No item selected</span>
      </div>
    </section>

    <aside class="inspector">
      <div class="insp-h">
        <h2 id="pvTitle">Details</h2>
        <p id="pvMeta">Select an item</p>
      </div>
      <div class="insp-b">
        <div class="stage-wrap">
          <div class="stage light" id="pvStage"><div class="hint">Select a file, then Preview (or right-click).</div></div>
          <div class="preview-bar" id="previewBar">
            <button class="tbtn danger" type="button" id="btnCancelPreview">Cancel preview</button>
            <span class="dim" id="previewState"></span>
          </div>
        </div>
        <div id="pvDetails"></div>
      </div>
      <div class="meter">
        <div id="meterLabel">Storage --</div>
        <div class="bar" id="meterTrack"><i id="meterBar"></i></div>
      </div>
    </aside>
  </div>
</div>
<div class="menu" id="menu"></div>
<div class="toast" id="toast"></div>
<input type="file" id="filePick" multiple hidden/>
<input type="file" id="folderPick" webkitdirectory directory multiple hidden/>
<div class="modal-bg" id="modalBg">
  <div class="modal" role="dialog" aria-modal="true">
    <div class="modal-h">
      <h3 id="modalTitle">Preview</h3>
      <button class="tbtn" type="button" id="modalDownload">Download</button>
      <button class="tbtn danger" type="button" id="modalCancel">Cancel</button>
      <button class="tbtn" type="button" id="modalClose">Close</button>
    </div>
    <div class="modal-b light" id="modalBody"><div class="hint">Loading...</div></div>
    <div class="modal-f">
      <span class="dim" id="modalState">Ready</span>
      <div class="spacer"></div>
      <span class="dim" id="modalMeta"></span>
    </div>
  </div>
</div>
<div class="login-bg" id="loginBg">
  <form class="login-box" id="loginForm">
    <h2>Sign in</h2>
    <p>Enter username and password for Web File Manager.</p>
    <label for="loginUser">Username</label>
    <input id="loginUser" autocomplete="username" required/>
    <label for="loginPass">Password</label>
    <input id="loginPass" type="password" autocomplete="current-password" required/>
    <div class="err" id="loginErr"></div>
    <div class="rowbtns">
      <button class="tbtn primary" type="submit" style="flex:1">Sign in</button>
    </div>
  </form>
</div>
<div class="dlg-bg" id="dlgBg" role="presentation">
  <div class="dlg" role="dialog" aria-modal="true" aria-labelledby="dlgTitle">
    <div class="dlg-h"><h2 id="dlgTitle">Notice</h2></div>
    <div class="dlg-b">
      <p id="dlgMsg"></p>
      <div id="dlgHint" class="dim" style="display:none"></div>
      <div id="dlgFieldWrap" style="display:none">
        <label id="dlgFieldLabel" for="dlgInput">Value</label>
        <input id="dlgInput" type="text" autocomplete="off"/>
      </div>
    </div>
    <div class="dlg-f">
      <button class="tbtn" type="button" id="dlgCancel">Cancel</button>
      <button class="tbtn primary" type="button" id="dlgOk">OK</button>
    </div>
  </div>
</div>
<script>
(function(){
var listEl=document.getElementById('list');
var menu=document.getElementById('menu');
var path='/', busy=false, entries=[], selected=null, clipboard=null, clipMode=null, marked={};
var searchMode=false, hist=[], histI=-1, blobUrl=null, suppressHist=false;
var previewToken=0, previewAbort=null, previewObjectUrl=null;
var sidePreviewTimer=null, sidePreviewItem=null;
var xferBusy=false;
var authUser=sessionStorage.getItem('wfm_user')||'';
var authPass=sessionStorage.getItem('wfm_pass')||'';

function fileBase(){var p=(window.WFM_FILE_PORT||81);return location.protocol+'//'+location.hostname+':'+p}
/** Upload always same-origin (port 80) - avoids CORS network errors. */
function uploadUrl(dir){return '/api/upload?dir='+encodeURIComponent(dir)}
/** Prefer transfer port; fall back to same-origin. */
function downloadUrl(path){return fileBase()+'/download?path='+encodeURIComponent(path)}
function downloadUrlLocal(path){return '/download?path='+encodeURIComponent(path)}
function viewUrl(path){
  // With auth, use same-origin so <img>/<video> work (no Auth header on media tags).
  if(window.WFM_AUTH) return '/view?path='+encodeURIComponent(path);
  return fileBase()+'/view?path='+encodeURIComponent(path);
}
function authHeader(){
  if(!window.WFM_AUTH) return null;
  if(!authUser) return null;
  return 'Basic '+btoa(unescape(encodeURIComponent(authUser+':'+authPass)));
}
function withAuth(opts){
  opts=opts||{};
  var h=Object.assign({}, opts.headers||{});
  var a=authHeader();
  if(a) h['Authorization']=a;
  opts.headers=h;
  return opts;
}
function esc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/"/g,'&quot;')}
function fmt(n){if(n==null||isNaN(n))return '--';var u=['B','KB','MB','GB','TB'],i=0,v=Number(n);while(v>=1024&&i<u.length-1){v/=1024;i++}return (i?v.toFixed(v>=10||i===1?0:1):v)+' '+u[i]}
function fmtSpeed(bps){if(!bps||bps<0||!isFinite(bps))return '--';if(bps>=1024*1024)return (bps/(1024*1024)).toFixed(2)+' MB/s';if(bps>=1024)return (bps/1024).toFixed(1)+' KB/s';return Math.round(bps)+' B/s'}
function fmtEta(sec){if(!isFinite(sec)||sec<0)return '';if(sec<1)return 'almost done';if(sec<60)return Math.ceil(sec)+'s left';var m=Math.floor(sec/60),s=Math.round(sec%60);return m+'m '+s+'s left'}
function extOf(n){var p=String(n).lastIndexOf('.');return p>=0?String(n).slice(p+1).toLowerCase():''}
function kindOf(name,isDir,isVol,vtype){
  if(isVol) return 'vol-'+(vtype||'vol');
  if(isDir) return 'dir';
  var e=extOf(name);
  if(['jpg','jpeg','png','gif','webp','bmp','svg'].indexOf(e)>=0) return 'img';
  if(['mp4','webm','mov','avi','mkv'].indexOf(e)>=0) return 'vid';
  if(['mp3','wav','ogg','aac','m4a','flac','opus'].indexOf(e)>=0) return 'aud';
  if(['txt','log','md','csv','json','xml','ini','cfg','nfo','srt','html','htm','css','js','cpp','cxx','cc','c','h','hpp','hh','ino','py','cmake','yml','yaml','toml','sh','bat','ps1','mak','mk'].indexOf(e)>=0) return 'txt';
  return 'file';
}
function typeLabel(name,isDir,isVol,vtype){
  if(isVol){
    if(vtype==='sd') return 'SD card volume';
    if(vtype==='flash') return 'Flash volume';
    return 'Storage volume';
  }
  if(isDir) return 'File folder';
  var e=extOf(name); if(!e) return 'File';
  var map={png:'PNG image',jpg:'JPEG image',jpeg:'JPEG image',gif:'GIF image',webp:'WEBP image',bmp:'Bitmap',
    mp4:'MP4 video',avi:'AVI video',mov:'MOV video',mkv:'MKV video',webm:'WEBM video',
    mp3:'MP3 audio',wav:'WAV audio',ogg:'OGG audio',aac:'AAC audio',m4a:'M4A audio',flac:'FLAC audio',opus:'Opus audio',
    txt:'Text Document',log:'Log File',json:'JSON File',csv:'CSV File',
    '3mf':'3MF File',gcode:'G-code File'};
  return map[e]||(e.toUpperCase()+' File');
}
function iconFolder(){return '<svg viewBox="0 0 16 16"><path d="M1.5 3.5h4.2l1.2 1.3H14.5v7.7H1.5z"/></svg>'}
function iconDrive(){return '<svg viewBox="0 0 16 16"><rect x="1" y="4" width="14" height="9" rx="1.2"/><path d="M4 11h3" stroke="#fff" stroke-width="1.4" fill="none"/></svg>'}
function iconVolume(vtype){
  if(vtype==='sd') return '<svg viewBox="0 0 16 16"><rect x="3" y="1.5" width="10" height="13" rx="1.2"/><path d="M5.5 4h5M5.5 6.5h5M5.5 9h3" stroke="#fff" stroke-width="1.2" fill="none"/></svg>';
  if(vtype==='flash') return '<svg viewBox="0 0 16 16"><path d="M7 1.5h2v3h3.5L8 14.5 3.5 4.5H7z"/></svg>';
  return iconDrive();
}
function iconFile(kind){
  if(kind==='img') return '<svg viewBox="0 0 16 16"><rect x="2" y="2" width="12" height="12" rx="1.5"/><circle cx="6" cy="6.5" r="1.3" fill="#fff"/><path d="m3.5 12.5 3.2-3.2 2.2 2.1 2-1.8 1.6 1.9" fill="#fff"/></svg>';
  if(kind==='vid') return '<svg viewBox="0 0 16 16"><rect x="1.5" y="3.5" width="9.5" height="9" rx="1.2"/><path d="M11.5 6.2 14.5 4.7v6.6L11.5 9.8z"/></svg>';
  if(kind==='aud') return '<svg viewBox="0 0 16 16"><path d="M6 12.5V4.2l7-1.4v8.4"/><circle cx="4.8" cy="12.5" r="1.6"/><circle cx="11.5" cy="11.2" r="1.6"/></svg>';
  if(kind==='txt') return '<svg viewBox="0 0 16 16"><path d="M3.5 1.5h6.2L12.5 4.3V14.5H3.5z"/><path d="M9.5 1.5V4.5H12.5M5.2 7.5h5.5M5.2 10h4.2" stroke="#fff" stroke-width="1" fill="none"/></svg>';
  return '<svg viewBox="0 0 16 16"><path d="M3.5 1.5h6.2L12.5 4.3V14.5H3.5z"/><path d="M9.5 1.5V4.5H12.5" stroke="#fff" stroke-width="1" fill="none"/></svg>';
}
function rowIcon(e){
  if(e.volume) return iconVolume(e.vtype||'vol');
  var k=kindOf(e.name,!!e.dir,false);
  return k==='dir'?iconFolder():iconFile(k);
}
function toast(m){var t=document.getElementById('toast');t.textContent=m;t.style.display='block';clearTimeout(t._t);t._t=setTimeout(function(){t.style.display='none'},2200)}

async function saveTextFile(path, text){
  var ctrl=new AbortController();
  var timer=setTimeout(function(){ctrl.abort()},30000);
  try{
    var headers={'Content-Type':'text/plain; charset=UTF-8'};
    var a=authHeader(); if(a) headers['Authorization']=a;
    var r=await fetch('/api/text/save?path='+encodeURIComponent(path),{
      method:'POST', headers:headers, body:text, signal:ctrl.signal, credentials:'omit'
    });
    clearTimeout(timer);
    var j=null;
    try{ j=await r.json(); }catch(e){}
    if(!r.ok) throw new Error((j&&j.error)||('HTTP '+r.status));
    return j;
  }catch(e){
    clearTimeout(timer);
    throw e;
  }
}

var _dlgResolve=null;
function closeDlg(result){
  document.getElementById('dlgBg').classList.remove('open');
  var r=_dlgResolve; _dlgResolve=null;
  if(r) r(result);
}
function openDlg(opts){
  opts=opts||{};
  return new Promise(function(resolve){
    if(_dlgResolve) closeDlg(null);
    _dlgResolve=resolve;
    document.getElementById('dlgTitle').textContent=opts.title||'Notice';
    document.getElementById('dlgMsg').textContent=opts.message||'';
    var hint=document.getElementById('dlgHint');
    if(opts.hint){ hint.style.display='block'; hint.textContent=opts.hint; }
    else { hint.style.display='none'; hint.textContent=''; }
    var wrap=document.getElementById('dlgFieldWrap');
    var input=document.getElementById('dlgInput');
    var cancel=document.getElementById('dlgCancel');
    var ok=document.getElementById('dlgOk');
    cancel.style.display=opts.showCancel===false?'none':'inline-flex';
    cancel.textContent=opts.cancelText||'Cancel';
    ok.textContent=opts.okText||'OK';
    ok.className='tbtn '+(opts.danger?'danger-fill':'primary');
    if(opts.prompt){
      wrap.style.display='block';
      document.getElementById('dlgFieldLabel').textContent=opts.fieldLabel||'Value';
      input.value=opts.value!=null?String(opts.value):'';
      input.placeholder=opts.placeholder||'';
      setTimeout(function(){ input.focus(); input.select(); },30);
    }else{
      wrap.style.display='none';
      setTimeout(function(){ ok.focus(); },30);
    }
    document.getElementById('dlgBg').classList.add('open');
  });
}
function uiAlert(message, title){
  return openDlg({title:title||'Notice', message:message, showCancel:false, okText:'OK'});
}
function uiConfirm(message, title, opts){
  opts=opts||{};
  return openDlg({
    title:title||'Confirm', message:message, hint:opts.hint||'',
    okText:opts.okText||'OK', cancelText:opts.cancelText||'Cancel', danger:!!opts.danger
  }).then(function(v){ return !!v; });
}
function uiPrompt(message, defaultValue, title, opts){
  opts=opts||{};
  return openDlg({
    title:title||'Input', message:message, hint:opts.hint||'',
    prompt:true, value:defaultValue, fieldLabel:opts.fieldLabel||'Value',
    placeholder:opts.placeholder||'', okText:opts.okText||'OK', cancelText:'Cancel'
  }).then(function(v){
    if(v===null||v===false) return null;
    return String(v);
  });
}
document.getElementById('dlgCancel').onclick=function(){closeDlg(null)};
document.getElementById('dlgOk').onclick=function(){
  var wrap=document.getElementById('dlgFieldWrap');
  if(wrap.style.display!=='none') closeDlg(document.getElementById('dlgInput').value);
  else closeDlg(true);
};
document.getElementById('dlgBg').addEventListener('click',function(ev){if(ev.target===this) closeDlg(null)});
document.getElementById('dlgInput').addEventListener('keydown',function(ev){
  if(ev.key==='Enter'){ev.preventDefault(); document.getElementById('dlgOk').click()}
  if(ev.key==='Escape'){ev.preventDefault(); closeDlg(null)}
});

function parentOf(p){var i=p.lastIndexOf('/');return i<=0?'/':p.slice(0,i)}
function join(dir,name){return dir==='/'?('/'+name):(dir+'/'+name)}
function fullOf(e){return e.path||((path==='/'?'':path)+'/'+e.name)}

function showLogin(msg){
  document.getElementById('loginBg').classList.add('open');
  document.getElementById('loginErr').textContent=msg||'';
  document.getElementById('loginUser').value=authUser||'';
  document.getElementById('loginPass').value='';
  document.getElementById('loginUser').focus();
}
function hideLogin(){document.getElementById('loginBg').classList.remove('open')}
async function ensureAuth(){
  if(!window.WFM_AUTH) return true;
  if(authUser && authPass){
    try{
      await fetchJson('/api/ping',8000);
      return true;
    }catch(e){
      if(String(e.message||'').indexOf('auth')>=0 || String(e.message||'').indexOf('401')>=0){
        authPass=''; sessionStorage.removeItem('wfm_pass');
      } else return true;
    }
  }
  return new Promise(function(resolve){
    showLogin('Sign in to continue');
    document.getElementById('loginForm').onsubmit=async function(ev){
      ev.preventDefault();
      authUser=document.getElementById('loginUser').value.trim();
      authPass=document.getElementById('loginPass').value;
      try{
        await fetchJson('/api/ping',8000);
        sessionStorage.setItem('wfm_user',authUser);
        sessionStorage.setItem('wfm_pass',authPass);
        hideLogin();
        resolve(true);
      }catch(e){
        document.getElementById('loginErr').textContent='Invalid username or password';
      }
    };
  });
}

async function fetchJson(url,ms,signal){
  ms=ms||20000;
  var local=null, sig=signal;
  if(!sig){local=new AbortController();sig=local.signal;setTimeout(function(){local.abort()},ms)}
  var r=await fetch(url, withAuth({cache:'no-store',signal:sig}));
  var text=await r.text(), j={};
  try{j=JSON.parse(text)}catch(e){throw new Error(r.status===401?'auth required':'Bad JSON')}
  if(r.status===401) throw new Error(j.error||'auth required');
  if(!r.ok) throw new Error(j.error||('HTTP '+r.status));
  return j;
}

function setPreviewBusy(on, msg){
  document.getElementById('previewBar').classList.toggle('on', !!on);
  document.getElementById('previewState').textContent=msg||'';
  document.getElementById('btnCancelPreview').disabled=!on;
  var ms=document.getElementById('modalState'); if(ms) ms.textContent=msg||'Ready';
}
function openModalShell(item){
  document.getElementById('modalTitle').textContent=item.name;
  document.getElementById('modalMeta').textContent=typeLabel(item.name,!!item.dir,!!item.volume,item.vtype)+' | '+fmt(item.size);
  document.getElementById('modalBg').classList.add('open');
  document.getElementById('modalBody').classList.add('light');
  document.getElementById('modalBody').innerHTML='<div class="loading"><div class="spin"></div>Loading preview...</div>';
  document.getElementById('modalDownload').onclick=function(){
    downloadFile(fullOf(item), item.name, item.size);
  };
}
function closeModal(){
  stopPreview();
  document.getElementById('modalBg').classList.remove('open');
  document.getElementById('modalBody').classList.add('light');
  document.getElementById('modalBody').innerHTML='<div class="hint">Closed.</div>';
}
function stopPreview(){
  previewToken++;
  if(previewAbort){try{previewAbort.abort()}catch(e){} previewAbort=null}
  var stage=document.getElementById('pvStage');
  var modal=document.getElementById('modalBody');
  [stage, modal].forEach(function(el){
    if(!el) return;
    var vid=el.querySelector('video,audio');
    if(vid){try{vid.pause();vid.removeAttribute('src');vid.load()}catch(e){}}
  });
  if(previewObjectUrl){URL.revokeObjectURL(previewObjectUrl);previewObjectUrl=null}
  if(blobUrl){URL.revokeObjectURL(blobUrl);blobUrl=null}
  var sideSave=document.getElementById('btnSideSaveTxt');
  if(sideSave) sideSave.style.display='none';
  setPreviewBusy(false,'');
}

function markedList(){
  var out=[], k;
  for(k in marked){ if(marked[k]) out.push(marked[k]); }
  return out;
}
function markCount(){ return markedList().length; }
function updateMarkUi(){
  var n=markCount();
  var bar=document.getElementById('selBar');
  bar.classList.toggle('on', n>0);
  document.getElementById('markMeta').textContent=n+' marked';
  document.getElementById('btnClearSel').disabled=n===0;
  document.getElementById('btnMoveMarked').disabled=n===0;
  var hasFiles=markedList().some(function(it){return !it.dir});
  document.getElementById('btnMarkDownload').disabled=!hasFiles;
  setRibbon();
}
function setMark(item, on){
  var key=fullOf(item);
  if(on) marked[key]=item; else delete marked[key];
  updateMarkUi();
}
function clearMarks(){ marked={}; updateMarkUi(); }
function setRibbon(){
  var has=!!selected;
  var n=markCount();
  var useMarked=n>0;
  document.getElementById('btnCut').disabled=!(useMarked||has);
  document.getElementById('btnCopy').disabled=!(useMarked||has);
  document.getElementById('btnPaste').disabled=!clipboard;
  document.getElementById('btnRename').disabled=!has || n>1;
  document.getElementById('btnDelete').disabled=!(useMarked||has) || !!(selected&&selected.volume&&!useMarked);
  document.getElementById('btnNewFolder').disabled=!canCreateHere(path);
  document.getElementById('btnNewFile').disabled=!canCreateHere(path);
  document.getElementById('btnPreview').disabled=!has||!!selected.dir||n>1;
  document.getElementById('btnDownload').disabled=useMarked ? !markedList().some(function(it){return !it.dir}) : (!has||!!selected.dir);
  document.getElementById('btnBack').disabled=histI<=0;
  document.getElementById('btnUp').disabled=(path==='/'&&!searchMode);
  document.getElementById('btnClearSel').disabled=n===0;
  document.getElementById('btnMoveMarked').disabled=n===0;
}

function setCrumbs(p){
  var parts=p==='/'?[]:p.replace(/^\/|\/$/g,'').split('/');
  var html='<a href="#" data-path="/">This SD</a>',acc='';
  for(var i=0;i<parts.length;i++){
    acc+='/'+parts[i];
    html+='<span class="sep">&gt;</span>';
    if(i===parts.length-1) html+='<span class="here">'+esc(parts[i])+'</span>';
    else html+='<a href="#" data-path="'+esc(acc)+'">'+esc(parts[i])+'</a>';
  }
  document.getElementById('crumbs').innerHTML=html;
  var links=document.getElementById('crumbs').querySelectorAll('a');
  for(var k=0;k<links.length;k++) links[k].onclick=function(ev){ev.preventDefault();searchMode=false;go(this.getAttribute('data-path'))};
  document.getElementById('winTitle').textContent=(p==='/'?'Web File Manager':parts[parts.length-1])+' - File Explorer';
  var sides=document.querySelectorAll('.side-item');
  for(var s=0;s<sides.length;s++) sides[s].classList.toggle('on', sides[s].getAttribute('data-path')===p);
}

async function loadStatus(){
  if(xferBusy) return;
  try{
    var j=await fetchJson('/api/status',8000);
    var track=document.getElementById('meterTrack'), bar=document.getElementById('meterBar'), label=document.getElementById('meterLabel');
    if(j.usage==='scanning'||j.usage==='pending'){
      track.classList.add('scan');
      label.textContent='Scanning storage...';
      document.getElementById('linkMeta').textContent='Ethernet '+(j.ip||'')+' | scanning';
    }else if(j.usage==='ready'&&j.total>0){
      track.classList.remove('scan');
      var pct=Math.min(100,Math.round(j.used*100/j.total));
      var free=j.total>j.used?(j.total-j.used):0;
      bar.style.width=pct+'%';
      label.textContent=fmt(j.used)+' used of '+fmt(j.total)+' ('+pct+'%)  |  '+fmt(free)+' free';
      document.getElementById('linkMeta').textContent='Ethernet '+j.ip+' | Online';
    }else{
      track.classList.remove('scan'); bar.style.width='0%';
      label.textContent=(j.card_mb||0)+' MB card';
      document.getElementById('linkMeta').textContent='Ethernet '+(j.ip||'');
    }
  }catch(e){document.getElementById('linkMeta').textContent='Status timeout'}
}

function clearPreview(){
  if(sidePreviewTimer){clearTimeout(sidePreviewTimer);sidePreviewTimer=null}
  sidePreviewItem=null;
  stopPreview();
  document.getElementById('pvTitle').textContent='Details';
  document.getElementById('pvMeta').textContent='Select an item';
  var stage=document.getElementById('pvStage');
  stage.classList.add('light');
  stage.innerHTML='<div class="hint">Select a file to preview it here.</div>';
  document.getElementById('pvDetails').innerHTML='';
  document.getElementById('selMeta').textContent='No item selected';
}

async function fillDetails(full){
  try{
    var j=await fetchJson('/api/details?path='+encodeURIComponent(full));
    document.getElementById('pvDetails').innerHTML='<dl class="details">'
      +'<dt>Name</dt><dd>'+esc(j.name)+'</dd>'
      +'<dt>Type</dt><dd>'+esc(j.dir?'File folder':j.mime)+'</dd>'
      +'<dt>Size</dt><dd>'+fmt(j.size)+' ('+j.size+' bytes)</dd>'
      +'<dt>Location</dt><dd>'+esc(j.parent)+'</dd>'
      +'<dt>Path</dt><dd>'+esc(j.path)+'</dd></dl>';
  }catch(e){}
}

function wireTextEditors(item, full, text, truncated, withModal){
  var stage=document.getElementById('pvStage');
  var modal=document.getElementById('modalBody');
  stage.classList.remove('light');
  stage.innerHTML='<textarea class="txt-edit" id="stageTxtEditor" spellcheck="false"></textarea>';
  if(withModal){
    modal.classList.remove('light');
    modal.innerHTML='<textarea class="txt-edit" id="modalTxtEditor" spellcheck="false"></textarea>'
      +'<div class="preview-bar on" style="margin-top:8px">'
      +'<button class="tbtn primary" type="button" id="btnSaveModalTxt">Save</button>'
      +'<span class="dim" id="modalTxtState">'+(truncated?'Read-only preview (truncated — download to edit full file)':'Editable · Save writes to SD')+'</span></div>';
  }
  var me=document.getElementById('modalTxtEditor');
  var se=document.getElementById('stageTxtEditor');
  if(me){ me.value=text; if(truncated) me.readOnly=true; }
  if(se){ se.value=text; if(truncated) se.readOnly=true; }
  function syncEditors(src){
    if(me && src!==me) me.value=src.value;
    if(se && src!==se) se.value=src.value;
  }
  if(me) me.oninput=function(){syncEditors(me)};
  if(se) se.oninput=function(){syncEditors(se)};
  async function doSave(){
    if(truncated){ toast('File truncated in preview — cannot save safely'); return; }
    var val=(me&&document.activeElement===me)?me.value:(se?se.value:(me?me.value:''));
    var st=document.getElementById('modalTxtState')||document.getElementById('previewState');
    if(st) st.textContent='Saving...';
    try{
      await saveTextFile(full, val);
      if(st) st.textContent='Saved';
      toast('Saved '+item.name);
    }catch(err){
      if(st) st.textContent='Save failed';
      toast(err.message||'save failed');
    }
  }
  var btn=document.getElementById('btnSaveModalTxt');
  if(btn){ btn.disabled=truncated; btn.onclick=doSave; }
  var bar=document.getElementById('previewBar');
  var sideSave=document.getElementById('btnSideSaveTxt');
  if(!sideSave){
    sideSave=document.createElement('button');
    sideSave.id='btnSideSaveTxt';
    sideSave.type='button';
    sideSave.className='tbtn primary';
    sideSave.textContent='Save';
    bar.insertBefore(sideSave, bar.firstChild);
  }
  sideSave.style.display=truncated?'none':'';
  sideSave.disabled=!!truncated;
  sideSave.onclick=doSave;
  document.getElementById('btnCancelPreview').textContent='Cancel preview';
  document.getElementById('btnCancelPreview').className='tbtn danger';
  document.getElementById('btnCancelPreview').onclick=cancelAllPreview;
  setPreviewBusy(true, truncated?'Read-only (truncated)':'Editable · Save to SD');
}

function showMediaPlayReady(item, kind){
  var stage=document.getElementById('pvStage');
  stage.classList.add('light');
  stage.innerHTML='<div class="play-ready">'
    +'<div>'+esc(kind==='vid'?'Video':'Audio')+' selected</div>'
    +'<div style="font-size:12px">Press Play to start streaming (saves bandwidth until then).</div>'
    +'<button class="tbtn primary" type="button" id="btnSidePlay">Play</button>'
    +'</div>';
  setPreviewBusy(false,'Waiting for Play');
  var btn=document.getElementById('btnSidePlay');
  if(btn) btn.onclick=function(){ playMediaPreview(item, false); };
}

function bindMediaPlayer(player, token, kind, modalEl){
  if(!player) return;
  var busyMsg=kind==='aud'?'Playing audio...':'Playing... Cancel anytime';
  player.addEventListener('loadstart',function(){if(token===previewToken) setPreviewBusy(true, kind==='aud'?'Buffering audio...':'Buffering... Cancel anytime')});
  if(kind==='vid'){
    player.addEventListener('loadedmetadata',function(){if(token===previewToken) setPreviewBusy(true,'Ready... Cancel anytime')});
  }
  player.addEventListener('canplay',function(){if(token===previewToken) setPreviewBusy(true,busyMsg)});
  player.addEventListener('playing',function(){if(token===previewToken) setPreviewBusy(true,busyMsg)});
  player.addEventListener('ended',function(){if(token===previewToken) setPreviewBusy(false,'Finished')});
  player.addEventListener('error',function(){
    if(token!==previewToken) return;
    var code=player.error?player.error.code:0;
    var why=kind==='aud'
      ? 'Unable to play this audio in the browser. Try Download, or use WAV/MP3.'
      : 'Unable to play this file in the browser.';
    if(kind==='vid'){
      if(code===2) why='Network error while streaming (Range/port 81). Try Download.';
      else if(code===3) why='Decode error - codec may be HEVC/H.265 or unsupported. Use Download or remux to H.264+AAC MP4.';
      else if(code===4) why='Format/codec not supported by this browser. Prefer H.264 + AAC MP4.';
    }
    if(modalEl){
      modalEl.classList.add('light');
      modalEl.innerHTML='<div class="hint">'+esc(why)+'</div>';
    }
    var stage=document.getElementById('pvStage');
    stage.classList.add('light');
    stage.innerHTML='<div class="hint">'+esc(why)+'</div>';
    setPreviewBusy(false,'Play failed');
  });
}

async function playMediaPreview(item, openModal){
  if(!item||item.dir||item.volume) return;
  stopPreview();
  var token=++previewToken;
  var kind=kindOf(item.name,false);
  var full=fullOf(item);
  var ext=extOf(item.name);
  var view=viewUrl(full);
  var stage=document.getElementById('pvStage');
  var modal=document.getElementById('modalBody');
  document.getElementById('pvTitle').textContent=item.name;
  document.getElementById('pvMeta').textContent=typeLabel(item.name,false)+' | '+fmt(item.size);
  if(openModal) openModalShell(item);
  if(kind==='aud'){
    var audOk=['wav','mp3','ogg','aac','m4a','flac','opus'];
    if(audOk.indexOf(ext)<0){
      var msg='This audio format cannot be previewed here. Use Download.';
      stage.classList.add('light'); stage.innerHTML='<div class="hint">'+esc(msg)+'</div>';
      if(openModal){ modal.classList.add('light'); modal.innerHTML='<div class="hint">'+esc(msg)+'</div>'; }
      setPreviewBusy(false,'Not previewable');
      return;
    }
    var mime={wav:'audio/wav',mp3:'audio/mpeg',ogg:'audio/ogg',aac:'audio/aac',m4a:'audio/mp4',flac:'audio/flac',opus:'audio/ogg'}[ext]||'audio/*';
    var audioHtml='<div style="padding:16px;width:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px">'
      +'<div style="color:#ddd;font-size:13px">'+esc(item.name)+'</div>'
      +'<audio id="sidePlayer" controls autoplay preload="auto" style="width:min(520px,92%)">'
      +'<source src="'+view+'" type="'+mime+'"/>'
      +'</audio></div>';
    stage.classList.remove('light');
    stage.innerHTML=audioHtml;
    if(openModal){
      modal.classList.remove('light');
      modal.innerHTML=audioHtml.replace('id="sidePlayer"','id="modalPlayer"');
      stage.innerHTML='<div class="hint">Audio playing in popup. Cancel preview to free the stream.</div>';
      bindMediaPlayer(document.getElementById('modalPlayer'), token, 'aud', modal);
    }else{
      bindMediaPlayer(document.getElementById('sidePlayer'), token, 'aud', null);
    }
    setPreviewBusy(true,'Buffering audio...');
    return;
  }
  if(kind==='vid'){
    if(!(ext==='mp4'||ext==='webm')){
      var vmsg='Only MP4/WEBM can be previewed here. Use Download.';
      stage.classList.add('light'); stage.innerHTML='<div class="hint">'+esc(vmsg)+'</div>';
      if(openModal){ modal.classList.add('light'); modal.innerHTML='<div class="hint">'+esc(vmsg)+'</div>'; }
      setPreviewBusy(false,'Not previewable');
      return;
    }
    // One stream only — dual video tags stall the single file server.
    if(openModal){
      modal.classList.remove('light');
      modal.innerHTML='<video id="modalPlayer" controls autoplay playsinline preload="auto" src="'+view+'"></video>';
      stage.classList.remove('light');
      stage.innerHTML='<div class="hint">Playing in popup. Cancel preview to free the stream.</div>';
      bindMediaPlayer(document.getElementById('modalPlayer'), token, 'vid', modal);
    }else{
      stage.classList.remove('light');
      stage.innerHTML='<video id="sidePlayer" controls autoplay playsinline preload="auto" src="'+view+'"></video>';
      bindMediaPlayer(document.getElementById('sidePlayer'), token, 'vid', null);
    }
    setPreviewBusy(true,'Buffering... Cancel anytime');
  }
}

async function sidePreview(item){
  if(!item){ clearPreview(); return; }
  if(item!==selected && item!==sidePreviewItem) return;
  stopPreview();
  var token=++previewToken;
  var kind=kindOf(item.name,!!item.dir,!!item.volume,item.vtype);
  var full=fullOf(item);
  var ext=extOf(item.name);
  var stage=document.getElementById('pvStage');
  document.getElementById('pvTitle').textContent=item.name;
  document.getElementById('pvMeta').textContent=typeLabel(item.name,!!item.dir,!!item.volume,item.vtype)+(item.dir||item.volume?'':(' | '+fmt(item.size)));
  document.getElementById('btnCancelPreview').textContent='Cancel preview';
  document.getElementById('btnCancelPreview').classList.add('danger');
  document.getElementById('btnCancelPreview').classList.remove('primary');
  document.getElementById('btnCancelPreview').onclick=function(){ cancelAllPreview(); };

  if(item.dir||item.volume){
    stage.classList.add('light');
    stage.innerHTML='<div class="hint">'+(item.volume?'Storage volume':'Folder')+' — double-click to open.</div>';
    setPreviewBusy(false,'');
    await fillDetails(full);
    return;
  }
  if(ext==='avi'||ext==='mkv'||ext==='mov'||ext==='mjpg'||ext==='mjpeg'){
    var blocked='This format cannot be previewed in the browser. Streaming AVI can freeze the board. Use Download.';
    stage.classList.add('light');
    stage.innerHTML='<div class="hint">'+esc(blocked)+'</div>';
    setPreviewBusy(false,'Not previewable');
    await fillDetails(full);
    return;
  }
  if(kind==='vid'||kind==='aud'){
    showMediaPlayReady(item, kind);
    await fillDetails(full);
    return;
  }
  previewAbort=new AbortController();
  var view=viewUrl(full);
  try{
    if(kind==='img'){
      setPreviewBusy(true,'Loading image...');
      stage.classList.remove('light');
      stage.innerHTML='<img id="sideImg" alt="'+esc(item.name)+'" src="'+view+'"/>';
      var img=document.getElementById('sideImg');
      if(img){
        img.onload=function(){ if(token===previewToken) setPreviewBusy(false,''); };
        img.onerror=function(){
          if(token!==previewToken) return;
          stage.classList.add('light');
          stage.innerHTML='<div class="hint">Image preview failed. Try Download.</div>';
          setPreviewBusy(false,'Preview failed');
        };
      }
    }else if(kind==='txt'){
      setPreviewBusy(true,'Loading text...');
      var j=await fetchJson('/api/text?path='+encodeURIComponent(full),25000,previewAbort.signal);
      if(token!==previewToken) return;
      wireTextEditors(item, full, j.text||'', !!j.truncated, false);
    }else{
      stage.classList.add('light');
      stage.innerHTML='<div class="hint">No preview for this type. Use Download or right-click.</div>';
      setPreviewBusy(false,'');
    }
    if(token===previewToken) await fillDetails(full);
  }catch(e){
    if(token!==previewToken) return;
    if(e && (e.name==='AbortError' || String(e.message||'').indexOf('abort')>=0)){
      setPreviewBusy(false,'Cancelled');
      return;
    }
    stage.classList.add('light');
    stage.innerHTML='<div class="hint">Preview failed: '+esc(e.message||e)+'</div>';
    setPreviewBusy(false,'');
  }
}

function queueSidePreview(item){
  sidePreviewItem=item;
  if(sidePreviewTimer) clearTimeout(sidePreviewTimer);
  sidePreviewTimer=setTimeout(function(){
    sidePreviewTimer=null;
    if(selected===item) sidePreview(item);
  }, 60);
}

async function showPreview(item){
  if(!item||item.dir||item.volume){toast('Folders have no preview');return}
  var kind=kindOf(item.name,false);
  var ext=extOf(item.name);
  document.getElementById('pvTitle').textContent=item.name;
  document.getElementById('pvMeta').textContent=typeLabel(item.name,false)+' | '+fmt(item.size);
  if(ext==='avi'||ext==='mkv'||ext==='mov'||ext==='mjpg'||ext==='mjpeg'){
    openModalShell(item);
    var msg='This format cannot be previewed in the browser. Streaming AVI can freeze the board. Use Download.';
    document.getElementById('modalBody').classList.add('light');
    document.getElementById('modalBody').innerHTML='<div class="hint">'+esc(msg)+'</div>';
    document.getElementById('pvStage').classList.add('light');
    document.getElementById('pvStage').innerHTML='<div class="hint">'+esc(msg)+'</div>';
    setPreviewBusy(false,'Not previewable');
    await fillDetails(fullOf(item));
    return;
  }
  if(kind==='vid'||kind==='aud'){
    await playMediaPreview(item, true);
    await fillDetails(fullOf(item));
    return;
  }
  stopPreview();
  var token=++previewToken;
  var full=fullOf(item);
  openModalShell(item);
  var stage=document.getElementById('pvStage');
  var modal=document.getElementById('modalBody');
  previewAbort=new AbortController();
  var view=viewUrl(full);
  try{
    if(kind==='img'){
      setPreviewBusy(true,'Loading...');
      modal.classList.remove('light');
      modal.innerHTML='<img id="modalImg" alt="'+esc(item.name)+'" src="'+view+'"/>';
      stage.classList.remove('light');
      stage.innerHTML='<div class="hint">Preview in popup</div>';
      var img=document.getElementById('modalImg');
      if(img){
        img.onload=function(){ if(token===previewToken) setPreviewBusy(false,''); };
        img.onerror=function(){
          if(token!==previewToken) return;
          modal.classList.add('light');
          modal.innerHTML='<div class="hint">JPG/image preview failed. Try Download.</div>';
          setPreviewBusy(false,'Preview failed');
        };
      }
    }else if(kind==='txt'){
      setPreviewBusy(true,'Loading...');
      var j=await fetchJson('/api/text?path='+encodeURIComponent(full),25000,previewAbort.signal);
      if(token!==previewToken) return;
      wireTextEditors(item, full, j.text||'', !!j.truncated, true);
    }else{
      modal.classList.add('light');
      modal.innerHTML='<div class="hint">No preview for this type. Use Download.</div>';
      stage.classList.add('light');
      stage.innerHTML='<div class="hint">No preview for this type.</div>';
      setPreviewBusy(false,'');
    }
    if(token===previewToken) await fillDetails(full);
  }catch(e){
    if(e && (e.name==='AbortError' || String(e.message||'').indexOf('abort')>=0)){
      if(token===previewToken){
        modal.classList.add('light');
        modal.innerHTML='<div class="hint">Preview cancelled.</div>';
        setPreviewBusy(false,'Cancelled');
      }
      return;
    }
    if(token!==previewToken) return;
    modal.classList.add('light');
    modal.innerHTML='<div class="hint">Preview failed: '+esc(e.message||e)+'</div>';
    setPreviewBusy(false,'');
  }
}
function showUploadProgress(on, label, pct, speedText, etaText){
  var panel=document.getElementById('uploadPanel');
  panel.classList.toggle('on', !!on);
  if(label!=null) document.getElementById('uploadLabel').textContent=label;
  if(pct!=null){
    var p=Math.max(0,Math.min(100,Math.round(pct)));
    document.getElementById('uploadPct').textContent=p+'%';
    document.getElementById('uploadBar').style.width=p+'%';
  }
  if(speedText!=null) document.getElementById('uploadSpeed').textContent=speedText;
  if(etaText!=null) document.getElementById('uploadEta').textContent=etaText;
  if(!on){
    document.getElementById('uploadSpeed').textContent='--';
    document.getElementById('uploadEta').textContent='';
  }
}
function xhrUpload(file, dir, progressCb){
  return new Promise(function(resolve, reject){
    var xhr=new XMLHttpRequest();
    var fd=new FormData();
    fd.append('file', file, file.name);
    var t0=performance.now(), lastT=t0, lastLoaded=0, instBps=0;
    xhr.open('POST', uploadUrl(dir));
    xhr.withCredentials=false;
    xhr.timeout=0;
    var a=authHeader(); if(a) xhr.setRequestHeader('Authorization', a);
    xhr.upload.onprogress=function(ev){
      if(!ev.lengthComputable) return;
      var now=performance.now();
      var dt=(now-lastT)/1000;
      if(dt>=0.25){
        instBps=(ev.loaded-lastLoaded)/dt;
        lastT=now; lastLoaded=ev.loaded;
      }
      var elapsed=Math.max(0.001,(now-t0)/1000);
      var avgBps=ev.loaded/elapsed;
      var bps=instBps>0?instBps:avgBps;
      var left=ev.total>ev.loaded?(ev.total-ev.loaded)/Math.max(bps,1):0;
      if(typeof progressCb==='function'){
        progressCb({loaded:ev.loaded,total:ev.total,bps:bps,avgBps:avgBps,eta:left});
      }else{
        showUploadProgress(true,
          'Uploading '+file.name+' ('+fmt(ev.loaded)+' / '+fmt(ev.total)+')',
          (ev.loaded/ev.total)*100, fmtSpeed(bps)+'  avg '+fmtSpeed(avgBps), fmtEta(left));
      }
    };
    xhr.onload=function(){
      var j={}; try{j=JSON.parse(xhr.responseText||'{}')}catch(e){}
      if(xhr.status>=200 && xhr.status<300 && j.ok) resolve(j);
      else reject(new Error(j.error||('HTTP '+xhr.status+' '+(xhr.responseText||'').slice(0,80))));
    };
    xhr.onerror=function(){reject(new Error('network error (connection dropped)'))};
    xhr.ontimeout=function(){reject(new Error('upload timeout'))};
    xhr.onabort=function(){reject(new Error('upload aborted'))};
    xhr.send(fd);
  });
}
async function ensureUploadDirs(targetDir){
  if(!targetDir || targetDir==='/' || targetDir===path) return;
  var base=path;
  var rel=targetDir;
  if(base!=='/' && targetDir.indexOf(base+'/')===0) rel=targetDir.slice(base.length);
  else if(base==='/' && targetDir.charAt(0)==='/') rel=targetDir;
  else return;
  if(rel.charAt(0)==='/') rel=rel.slice(1);
  if(!rel) return;
  var parts=rel.split('/').filter(Boolean);
  var cur=base;
  for(var i=0;i<parts.length;i++){
    var next=(cur==='/')?('/'+parts[i]):(cur+'/'+parts[i]);
    try{
      await fetchJson('/api/mkdir?dir='+encodeURIComponent(cur)+'&name='+encodeURIComponent(parts[i]));
    }catch(e){
      var msg=String(e.message||e);
      if(!/already exists|exists/i.test(msg)) throw e;
    }
    cur=next;
  }
}
async function uploadFiles(fileList){
  if(!fileList || !fileList.length) return;
  if(searchMode){toast('Open a folder before uploading');return}
  if(window.WFM_MULTI && path==='/'){toast('Open a volume (SD / LittleFS) before uploading');return}
  if(xferBusy){toast('Transfer already in progress');return}
  var files=[].slice.call(fileList);
  if(!files.length) return;
  var dir=path;
  var totalBytes=0;
  for(var t=0;t<files.length;t++) totalBytes+=files[t].size||0;
  var doneBytes=0;
  var ok=0, failed=[];
  xferBusy=true;
  showUploadProgress(true, 'Preparing '+files.length+' file'+(files.length>1?'s':'')+'...', 0, '--', '');
  for(var i=0;i<files.length;i++){
    var file=files[i];
    var rel=(file.webkitRelativePath||file.name||'').replace(/\\/g,'/');
    var parts=rel.split('/').filter(Boolean);
    var destDir=dir;
    if(parts.length>1){
      destDir=join(dir, parts.slice(0,-1).join('/'));
      try{ await ensureUploadDirs(destDir); }
      catch(e){
        failed.push(file.name+': mkdir '+(e.message||e));
        continue;
      }
    }
    try{
      await xhrUpload(file, destDir, function(p){
        var overall=totalBytes? ((doneBytes+p.loaded)/totalBytes)*100 : ((i+p.loaded/Math.max(p.total,1))/files.length)*100;
        showUploadProgress(true,
          'Uploading '+(i+1)+'/'+files.length+': '+file.name+' ('+fmt(p.loaded)+' / '+fmt(p.total)+')',
          overall, fmtSpeed(p.bps)+'  avg '+fmtSpeed(p.avgBps), fmtEta(p.eta));
      });
      doneBytes+=file.size||0;
      ok++;
    }catch(e){
      failed.push(file.name+': '+(e.message||e));
      showUploadProgress(true, 'Failed '+(i+1)+'/'+files.length+': '+file.name, totalBytes?(doneBytes/totalBytes)*100:0, '--', '');
    }
  }
  if(ok && !failed.length){
    showUploadProgress(true, 'Uploaded '+ok+' file'+(ok>1?'s':''), 100, '--', '');
    toast(ok>1?('Uploaded '+ok+' files'):('Uploaded '+files[0].name));
  }else if(ok && failed.length){
    showUploadProgress(true, 'Uploaded '+ok+', failed '+failed.length, 100, '--', '');
    toast('Uploaded '+ok+', failed '+failed.length);
    console.warn('Upload failures', failed);
  }else{
    showUploadProgress(true, 'Upload failed: '+(failed[0]||'unknown'), 0, '--', '');
    toast('Upload failed: '+(failed[0]||'unknown'));
  }
  setTimeout(function(){showUploadProgress(false); xferBusy=false}, failed.length&&!ok?2500:900);
  await go(path);
}
async function downloadFile(full, name, knownSize){
  xferBusy=true;
  showUploadProgress(true, 'Downloading '+name+'...', 0, '--', '');
  var t0=performance.now(), lastT=t0, lastLoaded=0, instBps=0, loaded=0;
  async function pull(url){
    var r=await fetch(url, withAuth({cache:'no-store',mode:'cors'}));
    if(r.status===401) throw new Error('auth required');
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r;
  }
  try{
    var r;
    try{ r=await pull(downloadUrl(full)); }
    catch(e1){ r=await pull(downloadUrlLocal(full)); }
    var total=knownSize||0;
    var cl=r.headers.get('Content-Length');
    if(cl) total=parseInt(cl,10)||total;
    if(!r.body || !r.body.getReader){
      var blob=await r.blob();
      var a=document.createElement('a');
      a.href=URL.createObjectURL(blob); a.download=name;
      document.body.appendChild(a); a.click(); a.remove();
      showUploadProgress(true, 'Downloaded '+name, 100, fmtSpeed(blob.size/Math.max(0.001,(performance.now()-t0)/1000)), '');
      setTimeout(function(){showUploadProgress(false); xferBusy=false}, 900);
      return;
    }
    var reader=r.body.getReader();
    var chunks=[];
    while(true){
      var step=await reader.read();
      if(step.done) break;
      chunks.push(step.value);
      loaded+=step.value.length;
      var now=performance.now();
      var dt=(now-lastT)/1000;
      if(dt>=0.25){ instBps=(loaded-lastLoaded)/dt; lastT=now; lastLoaded=loaded; }
      var elapsed=Math.max(0.001,(now-t0)/1000);
      var avgBps=loaded/elapsed;
      var bps=instBps>0?instBps:avgBps;
      var pct=total? (loaded/total)*100 : 0;
      var left=total>loaded?(total-loaded)/Math.max(bps,1):0;
      showUploadProgress(true,
        'Downloading '+name+' ('+fmt(loaded)+(total?' / '+fmt(total):'')+')',
        pct, fmtSpeed(bps)+'  avg '+fmtSpeed(avgBps), total?fmtEta(left):'');
    }
    var blob=new Blob(chunks);
    var a=document.createElement('a');
    a.href=URL.createObjectURL(blob); a.download=name;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(function(){URL.revokeObjectURL(a.href)}, 30000);
    showUploadProgress(true, 'Downloaded '+name, 100, fmtSpeed(loaded/Math.max(0.001,(performance.now()-t0)/1000)), '');
    setTimeout(function(){showUploadProgress(false); xferBusy=false}, 900);
  }catch(e){
    if(String(e.message||'').indexOf('auth')>=0){ showLogin('Sign in to download'); }
    showUploadProgress(true, 'Download failed: '+(e.message||e), 0, '--', '');
    toast('Download failed: '+(e.message||e));
    setTimeout(function(){showUploadProgress(false); xferBusy=false}, 2500);
  }
}
function canCreateHere(dir){
  if(searchMode) return false;
  if(window.WFM_MULTI && (!dir || dir==='/')) return false;
  return true;
}
async function createNewFolder(dir){
  dir=dir||path;
  if(!canCreateHere(dir)){
    await uiAlert('Open a volume folder (for example /SD) before creating items.', 'New folder');
    return;
  }
  var name=await uiPrompt('Create a new folder in:\n'+dir, 'New folder', 'New folder', {
    fieldLabel:'Folder name', okText:'Create', placeholder:'Folder name'
  });
  if(name===null) return;
  name=String(name||'').trim();
  if(!name) return;
  try{
    await fetchJson('/api/mkdir?dir='+encodeURIComponent(dir)+'&name='+encodeURIComponent(name));
    toast('Folder created');
    await go(dir);
  }catch(e){ await uiAlert(e.message||'Could not create folder', 'Error'); }
}
async function createNewFile(dir){
  dir=dir||path;
  if(!canCreateHere(dir)){
    await uiAlert('Open a volume folder (for example /SD) before creating items.', 'New file');
    return;
  }
  var name=await uiPrompt('Create an empty file in:\n'+dir, 'untitled.txt', 'New file', {
    fieldLabel:'File name', okText:'Create', placeholder:'name.txt'
  });
  if(name===null) return;
  name=String(name||'').trim();
  if(!name) return;
  try{
    await fetchJson('/api/create?dir='+encodeURIComponent(dir)+'&name='+encodeURIComponent(name));
    toast('File created');
    await go(dir);
  }catch(e){ await uiAlert(e.message||'Could not create file', 'Error'); }
}
async function runBatchDelete(items){
  if(!items.length) return;
  var vols=items.filter(function(it){return it.volume});
  if(vols.length){await uiAlert('Volume mounts cannot be deleted.', 'Not allowed');return}
  var hasFolder=items.some(function(it){return it.dir});
  var ok=await uiConfirm(
    'Permanently delete '+items.length+' item(s)?'+(hasFolder?'\n\nFolders will be removed with all contents.':'')+'\n\nThis cannot be undone.',
    hasFolder?'Delete items':'Delete',
    {okText:'Delete', danger:true, hint:items.slice(0,5).map(function(it){return it.name}).join(', ')+(items.length>5?' ...':'')}
  );
  if(!ok) return;
  for(var i=0;i<items.length;i++){
    try{ await fetchJson('/api/delete?path='+encodeURIComponent(fullOf(items[i]))); }
    catch(e){ await uiAlert('Delete failed: '+(e.message||e), 'Error'); break; }
  }
  clearMarks(); selected=null; clearPreview(); await go(path);
}
/* Browser-side ZIP (STORE) - ESP fetches files; PC builds the archive. */
var _crcTable=null;
function crc32Of(u8){
  if(!_crcTable){
    _crcTable=new Uint32Array(256);
    for(var n=0;n<256;n++){
      var c=n;
      for(var k=0;k<8;k++) c=(c&1)?(0xEDB88320^(c>>>1)):(c>>>1);
      _crcTable[n]=c>>>0;
    }
  }
  var crc=0xFFFFFFFF;
  for(var i=0;i<u8.length;i++) crc=_crcTable[(crc^u8[i])&0xFF]^(crc>>>8);
  return (crc^0xFFFFFFFF)>>>0;
}
function u16(n){return new Uint8Array([n&255,(n>>>8)&255])}
function u32(n){return new Uint8Array([n&255,(n>>>8)&255,(n>>>16)&255,(n>>>24)&255])}
function encName(s){
  try{return new TextEncoder().encode(s)}catch(e){
    var a=new Uint8Array(s.length); for(var i=0;i<s.length;i++) a[i]=s.charCodeAt(i)&255; return a;
  }
}
function buildZipStore(entries){
  // entries: [{name, data:Uint8Array}]
  var locals=[], central=[], offset=0;
  for(var i=0;i<entries.length;i++){
    var name=encName(entries[i].name);
    var data=entries[i].data;
    var crc=crc32Of(data);
    var size=data.length>>>0;
    var lfh=new Uint8Array(30+name.length);
    lfh.set(u32(0x04034b50),0);
    lfh.set(u16(20),4);
    lfh.set(u16(0),6);
    lfh.set(u16(0),8); // store
    lfh.set(u16(0),10);
    lfh.set(u16(0),12);
    lfh.set(u32(crc),14);
    lfh.set(u32(size),18);
    lfh.set(u32(size),22);
    lfh.set(u16(name.length),26);
    lfh.set(u16(0),28);
    lfh.set(name,30);
    locals.push(lfh, data);
    var cdh=new Uint8Array(46+name.length);
    cdh.set(u32(0x02014b50),0);
    cdh.set(u16(20),4);
    cdh.set(u16(20),6);
    cdh.set(u16(0),8);
    cdh.set(u16(0),10);
    cdh.set(u16(0),12);
    cdh.set(u16(0),14);
    cdh.set(u32(crc),16);
    cdh.set(u32(size),20);
    cdh.set(u32(size),24);
    cdh.set(u16(name.length),28);
    cdh.set(u16(0),30);
    cdh.set(u16(0),32);
    cdh.set(u16(0),34);
    cdh.set(u16(0),36);
    cdh.set(u32(0),38);
    cdh.set(u32(offset),42);
    cdh.set(name,46);
    central.push(cdh);
    offset+=lfh.length+data.length;
  }
  var cdSize=0; for(var j=0;j<central.length;j++) cdSize+=central[j].length;
  var eocd=new Uint8Array(22);
  eocd.set(u32(0x06054b50),0);
  eocd.set(u16(0),4);
  eocd.set(u16(0),6);
  eocd.set(u16(entries.length),8);
  eocd.set(u16(entries.length),10);
  eocd.set(u32(cdSize),12);
  eocd.set(u32(offset),16);
  eocd.set(u16(0),20);
  var parts=locals.concat(central); parts.push(eocd);
  return new Blob(parts,{type:'application/zip'});
}
async function fetchFileBytes(full, name, knownSize, fileIndex, fileCount){
  async function pull(url){
    var r=await fetch(url, withAuth({cache:'no-store',mode:'cors'}));
    if(r.status===401) throw new Error('auth required');
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r;
  }
  var r;
  try{ r=await pull(downloadUrl(full)); }
  catch(e1){ r=await pull(downloadUrlLocal(full)); }
  var total=knownSize||0;
  var cl=r.headers.get('Content-Length');
  if(cl) total=parseInt(cl,10)||total;
  if(!r.body || !r.body.getReader){
    var ab=await r.arrayBuffer();
    showUploadProgress(true, 'Fetched '+(fileIndex+1)+'/'+fileCount+': '+name, ((fileIndex+1)/fileCount)*100, '--', '');
    return new Uint8Array(ab);
  }
  var reader=r.body.getReader();
  var chunks=[];
  var loaded=0;
  var t0=performance.now(), lastT=t0, lastLoaded=0, instBps=0;
  while(true){
    var step=await reader.read();
    if(step.done) break;
    chunks.push(step.value);
    loaded+=step.value.length;
    var now=performance.now();
    var dt=(now-lastT)/1000;
    if(dt>=0.25){ instBps=(loaded-lastLoaded)/dt; lastT=now; lastLoaded=loaded; }
    var elapsed=Math.max(0.001,(now-t0)/1000);
    var avgBps=loaded/elapsed;
    var bps=instBps>0?instBps:avgBps;
    var pctFile=total?(loaded/total)*100:0;
    var pct=((fileIndex+pctFile/100)/fileCount)*100;
    var left=total>loaded?(total-loaded)/Math.max(bps,1):0;
    showUploadProgress(true,
      'Fetching '+(fileIndex+1)+'/'+fileCount+': '+name+'  '+fmt(loaded)+(total?' / '+fmt(total):''),
      pct, fmtSpeed(bps)+'  avg '+fmtSpeed(avgBps), total?fmtEta(left):'');
  }
  var out=new Uint8Array(loaded);
  var o=0;
  for(var i=0;i<chunks.length;i++){ out.set(chunks[i], o); o+=chunks[i].length; }
  return out;
}
async function downloadZip(files){
  if(xferBusy){toast('Transfer already in progress');return}
  if(!files || !files.length) return;
  xferBusy=true;
  var zipName='download.zip';
  if(files.length<=3){
    zipName=files.map(function(it){return (it.name||'file').replace(/\.[^.]+$/,'')}).join('_').slice(0,40)+'.zip';
  }
  showUploadProgress(true, 'Browser zip: fetching '+files.length+' file(s)...', 0, '--', '');
  try{
    var used={};
    var entries=[];
    for(var i=0;i<files.length;i++){
      var it=files[i];
      var nm=it.name||('file'+i);
      var base=nm, n=1;
      while(used[base]){ var dot=nm.lastIndexOf('.'); base=(dot>0?(nm.slice(0,dot)+'_'+n+nm.slice(dot)):(nm+'_'+n)); n++; }
      used[base]=1;
      var data=await fetchFileBytes(fullOf(it), nm, it.size||0, i, files.length);
      entries.push({name:base, data:data});
    }
    showUploadProgress(true, 'Building zip in browser...', 95, '--', '');
    await new Promise(function(r){ setTimeout(r, 20); });
    var blob=buildZipStore(entries);
    var a=document.createElement('a');
    a.href=URL.createObjectURL(blob); a.download=zipName;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(function(){URL.revokeObjectURL(a.href)}, 30000);
    showUploadProgress(true, 'Downloaded '+zipName+' ('+fmt(blob.size)+')', 100, '--', '');
    setTimeout(function(){showUploadProgress(false); xferBusy=false}, 900);
    toast('Zip ready ('+files.length+' files)');
  }catch(e){
    if(String(e.message||'').indexOf('auth')>=0){ showLogin('Sign in to download'); }
    showUploadProgress(true, 'Zip failed: '+(e.message||e), 0, '--', '');
    toast('Zip failed: '+(e.message||e));
    setTimeout(function(){showUploadProgress(false); xferBusy=false}, 2500);
  }
}
async function runBatchDownload(items){
  var files=items.filter(function(it){return !it.dir && !it.volume});
  if(!files.length){await uiAlert('No files marked for download.', 'Download');return}
  if(files.length===1){
    await downloadFile(fullOf(files[0]), files[0].name, files[0].size);
    return;
  }
  if(files.length>48){
    await uiAlert('Too many files selected (max 48). Mark fewer items and try again.', 'Download');
    return;
  }
  await downloadZip(files);
}
async function runBatchMove(items){
  if(!items.length) return;
  var destDir=await uiPrompt(
    'Enter the destination folder path for '+items.length+' marked item(s).',
    path,
    'Move items',
    {fieldLabel:'Destination folder', okText:'Move', hint:'Example: /SD/backup'}
  );
  if(destDir===null) return;
  destDir=String(destDir||'').trim();
  if(!destDir) return;
  if(destDir.charAt(0)!=='/') destDir='/'+destDir;
  for(var i=0;i<items.length;i++){
    var it=items[i];
    var dest=join(destDir, it.name);
    try{ await fetchJson('/api/move?from='+encodeURIComponent(fullOf(it))+'&to='+encodeURIComponent(dest)); }
    catch(e){ await uiAlert('Move failed: '+(e.message||e), 'Error'); break; }
  }
  clearMarks(); selected=null; clearPreview(); await go(path); toast('Move done');
}

function selectItem(item,rowEl){
  selected=item;
  var rows=listEl.querySelectorAll('.row');
  for(var i=0;i<rows.length;i++) rows[i].classList.remove('on');
  if(rowEl) rowEl.classList.add('on');
  document.getElementById('selMeta').textContent=item?(item.name+' selected'):'No item selected';
  document.getElementById('pvTitle').textContent=item?item.name:'Details';
  document.getElementById('pvMeta').textContent=item?(typeLabel(item.name,!!item.dir,!!item.volume,item.vtype)+(item.dir||item.volume?'':(' | '+fmt(item.size)))):'Select an item';
  setRibbon();
  if(item) queueSidePreview(item);
  else clearPreview();
}

function hideMenu(){menu.classList.remove('open')}
function openMenu(x,y,item){
  selectItem(item,null);
  var rows=listEl.querySelectorAll('.row');
  for(var i=0;i<rows.length;i++) if(rows[i].getAttribute('data-key')===fullOf(item)) rows[i].classList.add('on');
  var canPrev=!item.dir && ['img','vid','aud','txt'].indexOf(kindOf(item.name,false))>=0;
  var canEdit=!item.dir && kindOf(item.name,false)==='txt';
  var html='';
  if(item.dir) html+='<button type="button" data-act="open">Open</button>';
  else html+='<button type="button" data-act="preview"'+(canPrev?'':' disabled')+'>Preview</button>';
  if(canEdit) html+='<button type="button" data-act="edit">Edit</button>';
  html+='<button type="button" data-act="details">Properties</button><div class="sep"></div>';
  html+='<button type="button" data-act="cut">Cut</button><button type="button" data-act="copy">Copy</button>';
  if(clipboard) html+='<button type="button" data-act="paste">Paste</button>';
  html+='<div class="sep"></div><button type="button" data-act="rename">Rename</button><button type="button" data-act="move">Move to...</button>';
  if(!item.dir) html+='<button type="button" data-act="download">Download</button>';
  html+='<button type="button" data-act="upload">Upload files here</button>';
  html+='<button type="button" data-act="uploadfolder">Upload folder here</button>';
  if(item.dir && !item.volume){
    html+='<div class="sep"></div><button type="button" data-act="newfolder">New folder here</button><button type="button" data-act="newfile">New file here</button>';
  }
  html+='<div class="sep"></div><button type="button" class="danger" data-act="delete">'+(item.dir?'Delete folder':'Delete')+'</button>';
  menu.innerHTML=html; menu.classList.add('open');
  var mw=menu.offsetWidth, mh=menu.offsetHeight;
  menu.style.left=Math.max(8,Math.min(x,window.innerWidth-mw-8))+'px';
  menu.style.top=Math.max(8,Math.min(y,window.innerHeight-mh-8))+'px';
  var btns=menu.querySelectorAll('button[data-act]');
  for(var b=0;b<btns.length;b++) btns[b].onclick=function(){var a=this.getAttribute('data-act');hideMenu();runAction(a,item)};
}

async function runAction(act,item){
  if(!item && act!=='paste' && act!=='newfolder' && act!=='newfile') return;
  var full=item?fullOf(item):path;
  try{
    if(act==='open'){searchMode=false;await go(full);return}
    if(act==='preview' || act==='edit'){await showPreview(item);return}
    if(act==='details'){selectItem(item,null);await fillDetails(full);document.getElementById('pvStage').classList.add('light');document.getElementById('pvStage').innerHTML='<div class="hint">Properties shown below.</div>';return}
    if(act==='download'){await downloadFile(full, item.name, item.size);return}
    if(act==='upload'){
      if(item.dir){path=full; setCrumbs(path)}
      document.getElementById('filePick').click();
      return;
    }
    if(act==='uploadfolder'){
      if(item.dir){path=full; setCrumbs(path)}
      document.getElementById('folderPick').click();
      return;
    }
    if(act==='newfolder'){
      var here=(item&&item.dir&&!item.volume)?full:path;
      await createNewFolder(here); return;
    }
    if(act==='newfile'){
      var here2=(item&&item.dir&&!item.volume)?full:path;
      await createNewFile(here2); return;
    }
    if(act==='copy'||act==='cut'){
      if(item.volume){await uiAlert('Volumes cannot be cut or copied.', 'Not allowed');return}
      clipboard={from:full,name:item.name,dir:!!item.dir};clipMode=act;toast(act==='cut'?'Cut':'Copied');setRibbon();return;
    }
    if(act==='paste'){
      if(!clipboard) return;
      if(clipboard.dir){toast('Folder copy not supported');return}
      var destDir=(item&&item.dir)?fullOf(item):path;
      var dest=join(destDir,clipboard.name);
      if(clipMode==='cut'){await fetchJson('/api/move?from='+encodeURIComponent(clipboard.from)+'&to='+encodeURIComponent(dest));clipboard=null;clipMode=null;toast('Moved')}
      else {await fetchJson('/api/copy?from='+encodeURIComponent(clipboard.from)+'&to='+encodeURIComponent(dest));toast('Pasted')}
      await go(path); return;
    }
    if(act==='move'){
      if(item.volume){await uiAlert('Volumes cannot be moved.', 'Not allowed');return}
      var dest=await uiPrompt(
        'Move "'+item.name+'" to a new full path.',
        join(path, item.name),
        'Move item',
        {fieldLabel:'Destination path', okText:'Move'}
      );
      if(dest===null) return;
      dest=String(dest||'').trim();
      if(!dest) return;
      await fetchJson('/api/move?from='+encodeURIComponent(full)+'&to='+encodeURIComponent(dest));
      toast('Moved'); selected=null; clearPreview(); await go(path); return;
    }
    if(act==='rename'){
      if(item.volume){await uiAlert('Volumes cannot be renamed.', 'Not allowed');return}
      var nn=await uiPrompt(
        'Enter a new name for this item.',
        item.name,
        'Rename',
        {fieldLabel:'New name', okText:'Rename'}
      );
      if(nn===null) return;
      nn=String(nn||'').trim();
      if(!nn||nn===item.name) return;
      await fetchJson('/api/rename?path='+encodeURIComponent(full)+'&name='+encodeURIComponent(nn));
      toast('Renamed'); selected=null; clearPreview(); await go(path); return;
    }
    if(act==='delete'){
      if(item.volume){await uiAlert('Volumes cannot be deleted from here.', 'Not allowed');return}
      var msg=item.dir
        ? ('Permanently delete this folder and everything inside?\n\n'+full)
        : ('Permanently delete this file?\n\n'+full);
      var ok=await uiConfirm(msg, item.dir?'Delete folder':'Delete', {
        okText:'Delete', danger:true, hint:'This cannot be undone.'
      });
      if(!ok) return;
      await fetchJson('/api/delete?path='+encodeURIComponent(full));
      toast(item.dir?'Folder deleted':'Deleted'); selected=null; clearPreview(); await go(path); return;
    }
  }catch(e){await uiAlert(e.message||'Action failed', 'Error')}
}

function renderQuick(rootEntries){
  var q=document.getElementById('quick'), html='';
  for(var i=0;i<rootEntries.length;i++){
    if(!rootEntries[i].dir) continue;
    var full='/'+rootEntries[i].name;
    var vt=rootEntries[i].vtype||(rootEntries[i].volume?'vol':'');
    var ic=rootEntries[i].volume?iconVolume(vt):iconFolder();
    var cls=rootEntries[i].volume?('vol-'+(vt||'vol')):'folder';
    html+='<div class="side-item" data-path="'+esc(full)+'"><span class="si '+cls+'">'+ic+'</span><span>'+esc(rootEntries[i].name)+'</span></div>';
  }
  q.innerHTML=html;
  var items=q.querySelectorAll('.side-item');
  for(var k=0;k<items.length;k++) items[k].onclick=function(){searchMode=false;go(this.getAttribute('data-path'))};
}

function render(){
  if(!entries.length){
    listEl.innerHTML='<div class="empty">'+(searchMode?'No items match your search.':(window.WFM_MULTI&&path==='/'?'No volumes mounted.':'This folder is empty.'))+'</div>';
    document.getElementById('countMeta').textContent='0 items';
    document.getElementById('chkAll').checked=false;
    updateMarkUi(); return;
  }
  var dirs=[],files=[],i;
  for(i=0;i<entries.length;i++){ if(entries[i].dir) dirs.push(entries[i]); else files.push(entries[i]); }
  dirs.sort(function(a,b){return a.name.localeCompare(b.name)});
  files.sort(function(a,b){return a.name.localeCompare(b.name)});
  var all=dirs.concat(files), html='';
  for(i=0;i<all.length;i++){
    var e=all[i]; e.full=fullOf(e);
    var kind=kindOf(e.name,!!e.dir,!!e.volume,e.vtype);
    var isMarked=!!marked[e.full];
    html+='<div class="row'+(isMarked?' marked':'')+'" data-key="'+esc(e.full)+'" data-idx="'+i+'">'
      +'<div class="chk"><input type="checkbox" '+(isMarked?'checked ':'')+'/></div>'
      +'<div class="c name"><span class="fi '+kind+'">'+rowIcon(e)+'</span><span>'+esc(e.name)+'</span></div>'
      +'<div class="c dim">'+esc(typeLabel(e.name,!!e.dir,!!e.volume,e.vtype))+'</div>'
      +'<div class="c dim">'+(e.dir?'':fmt(e.size))+'</div></div>';
  }
  listEl._items=all; listEl.innerHTML=html;
  var rows=listEl.querySelectorAll('.row');
  for(i=0;i<rows.length;i++){
    (function(row){
      function itemOf(){return listEl._items[Number(row.getAttribute('data-idx'))]}
      var cb=row.querySelector('input[type=checkbox]');
      cb.onclick=function(ev){ev.stopPropagation(); setMark(itemOf(), cb.checked); row.classList.toggle('marked', cb.checked)};
      row.onclick=function(ev){
        if(ev.target && ev.target.type==='checkbox') return;
        ev.preventDefault(); hideMenu();
        if(ev.ctrlKey || ev.metaKey){
          var it=itemOf(); var on=!marked[fullOf(it)];
          setMark(it,on); cb.checked=on; row.classList.toggle('marked', on); selectItem(it,row); return;
        }
        selectItem(itemOf(),row);
      };
      row.oncontextmenu=function(ev){ev.preventDefault();openMenu(ev.clientX,ev.clientY,itemOf())};
      row.ondblclick=function(ev){ev.preventDefault();hideMenu();var it=itemOf();if(it.dir){searchMode=false;go(it.full)}else showPreview(it)};
    })(rows[i]);
  }
  document.getElementById('countMeta').textContent=all.length+(all.length===1?' item':' items')+(window._trunc?' (truncated)':'');
  document.getElementById('chkAll').checked = all.length>0 && all.every(function(e){return !!marked[fullOf(e)]});
  updateMarkUi();
}

function pushHist(p){if(suppressHist)return;if(histI>=0&&hist[histI]===p)return;hist=hist.slice(0,histI+1);hist.push(p);histI=hist.length-1}
async function go(p){
  if(busy) return;
  busy=true; searchMode=false; selected=null; clearMarks(); stopPreview();
  path=p||'/'; if(path.charAt(0)!=='/') path='/'+path;
  pushHist(path); setCrumbs(path); clearPreview(); setRibbon();
  listEl.innerHTML='<div class="loading"><div class="spin"></div>'+(xferBusy?'Loading (transfer running)...':'Loading...')+'</div>';
  try{
    var j=await fetchJson('/api/list?path='+encodeURIComponent(path), xferBusy?45000:20000);
    entries=j.entries||[]; window._trunc=!!j.truncated; render();
    if(path==='/') renderQuick(entries);
  }catch(e){
    var msg=String(e.message||e);
    if(xferBusy || /abort/i.test(msg))
      msg='Folder busy during transfer - click Refresh in a moment.';
    listEl.innerHTML='<div class="empty">Could not open folder.<br>'+esc(msg)+'</div>';
  }
  finally{busy=false; setRibbon()}
}
async function runSearch(q){
  q=(q||'').trim(); if(!q){await uiAlert('Type a file name to search.', 'Search');return}
  if(busy) return;
  busy=true; searchMode=true; selected=null; clearPreview();
  document.getElementById('winTitle').textContent='Search Results - File Explorer';
  listEl.innerHTML='<div class="loading"><div class="spin"></div>Searching...</div>';
  try{
    var j=await fetchJson('/api/search?q='+encodeURIComponent(q)+'&path=/',60000);
    entries=(j.results||[]).map(function(r){return {name:r.name,path:r.path,dir:r.dir,size:r.size}});
    window._trunc=!!j.truncated; render();
    document.getElementById('countMeta').textContent=(j.count||0)+' results';
    toast((j.count||0)+' result(s)');
  }catch(e){listEl.innerHTML='<div class="empty">Search failed.<br>'+esc(e.message||e)+'</div>'}
  finally{busy=false; setRibbon()}
}

function cancelAllPreview(){
  stopPreview();
  var stage=document.getElementById('pvStage');
  stage.classList.add('light');
  stage.innerHTML='<div class="hint">Preview cancelled.</div>';
  var modal=document.getElementById('modalBody');
  if(modal){modal.classList.add('light'); modal.innerHTML='<div class="hint">Preview cancelled.</div>';}
  toast('Preview cancelled');
}
document.getElementById('btnCancelPreview').onclick=cancelAllPreview;
document.getElementById('modalCancel').onclick=cancelAllPreview;
document.getElementById('modalClose').onclick=function(){closeModal()};
document.getElementById('modalBg').addEventListener('click',function(ev){if(ev.target===this) closeModal()});
document.getElementById('btnUpload').onclick=function(){document.getElementById('filePick').click()};
document.getElementById('btnUploadFolder').onclick=function(){document.getElementById('folderPick').click()};
document.getElementById('btnNewFolder').onclick=function(){createNewFolder(path)};
document.getElementById('btnNewFile').onclick=function(){createNewFile(path)};
document.getElementById('filePick').onchange=function(){uploadFiles(this.files); this.value='';};
document.getElementById('folderPick').onchange=function(){uploadFiles(this.files); this.value='';};
(function setupDropUpload(){
  var pane=document.getElementById('mainPane');
  if(!pane) return;
  var dragDepth=0;
  function canDrop(){
    if(xferBusy||searchMode) return false;
    if(window.WFM_MULTI && path==='/') return false;
    return true;
  }
  function setDrop(on){ pane.classList.toggle('drop-target', !!on); }
  pane.addEventListener('dragenter',function(ev){
    if(!ev.dataTransfer||!ev.dataTransfer.types||ev.dataTransfer.types.indexOf('Files')<0) return;
    ev.preventDefault(); dragDepth++; setDrop(canDrop());
  });
  pane.addEventListener('dragover',function(ev){
    if(!ev.dataTransfer||!ev.dataTransfer.types||ev.dataTransfer.types.indexOf('Files')<0) return;
    ev.preventDefault();
    ev.dataTransfer.dropEffect=canDrop()?'copy':'none';
    setDrop(canDrop());
  });
  pane.addEventListener('dragleave',function(ev){
    ev.preventDefault();
    dragDepth=Math.max(0,dragDepth-1);
    if(!dragDepth) setDrop(false);
  });
  pane.addEventListener('drop',function(ev){
    ev.preventDefault(); dragDepth=0; setDrop(false);
    if(!canDrop()){
      if(window.WFM_MULTI && path==='/') toast('Open a volume folder before uploading');
      else if(searchMode) toast('Open a folder before uploading');
      else toast('Transfer already in progress');
      return;
    }
    var files=ev.dataTransfer&&ev.dataTransfer.files;
    if(files&&files.length) uploadFiles(files);
  });
})();
document.getElementById('btnRefresh').onclick=function(){go(path);loadStatus()};
document.getElementById('btnHome').onclick=function(){searchMode=false;go('/')};
document.getElementById('btnUp').onclick=function(){
  if(searchMode){go(path);return}
  if(path==='/') return;
  var parts=path.replace(/\/$/,'').split('/'); parts.pop();
  go(parts.length<=1?'/':parts.join('/'));
};
document.getElementById('btnBack').onclick=function(){
  if(histI<=0) return;
  histI--; suppressHist=true; go(hist[histI]).then(function(){suppressHist=false});
};
document.getElementById('btnPreview').onclick=function(){if(selected) showPreview(selected)};
document.getElementById('btnDownload').onclick=function(){
  var items=markCount()?markedList():(selected?[selected]:[]);
  runBatchDownload(items);
};
document.getElementById('btnCopy').onclick=function(){
  var items=markCount()?markedList():(selected?[selected]:[]);
  if(!items.length) return;
  clipboard={multi:true, items:items.map(function(it){return {from:fullOf(it),name:it.name,dir:!!it.dir}})};
  clipMode='copy'; toast('Copied '+items.length+' item(s)'); setRibbon();
};
document.getElementById('btnCut').onclick=function(){
  var items=markCount()?markedList():(selected?[selected]:[]);
  if(!items.length) return;
  clipboard={multi:true, items:items.map(function(it){return {from:fullOf(it),name:it.name,dir:!!it.dir}})};
  clipMode='cut'; toast('Cut '+items.length+' item(s)'); setRibbon();
};
document.getElementById('btnPaste').onclick=async function(){
  if(!clipboard) return;
  var destDir=(selected&&selected.dir)?fullOf(selected):path;
  if(clipboard.multi && clipboard.items){
    for(var i=0;i<clipboard.items.length;i++){
      var it=clipboard.items[i];
      if(it.dir){toast('Folder copy not supported'); continue;}
      var dest=join(destDir, it.name);
      try{
        if(clipMode==='cut') await fetchJson('/api/move?from='+encodeURIComponent(it.from)+'&to='+encodeURIComponent(dest));
        else await fetchJson('/api/copy?from='+encodeURIComponent(it.from)+'&to='+encodeURIComponent(dest));
      }catch(e){toast(e.message||'Paste failed'); break;}
    }
    if(clipMode==='cut'){clipboard=null; clipMode=null;}
    clearMarks(); await go(path); toast('Paste done'); return;
  }
  runAction('paste', selected&&selected.dir?selected:{name:'',dir:true,path:path});
};
document.getElementById('btnRename').onclick=function(){if(selected) runAction('rename',selected)};
document.getElementById('btnDelete').onclick=function(){
  var items=markCount()?markedList():(selected?[selected]:[]);
  runBatchDelete(items);
};
document.getElementById('btnSelectAll').onclick=function(){
  entries.forEach(function(e){ e.full=fullOf(e); marked[e.full]=e; });
  render();
};
document.getElementById('btnClearSel').onclick=function(){clearMarks(); render();};
document.getElementById('btnMoveMarked').onclick=function(){runBatchMove(markedList())};
document.getElementById('btnMarkCopy').onclick=function(){document.getElementById('btnCopy').click()};
document.getElementById('btnMarkCut').onclick=function(){document.getElementById('btnCut').click()};
document.getElementById('btnMarkDownload').onclick=function(){runBatchDownload(markedList())};
document.getElementById('btnMarkDelete').onclick=function(){runBatchDelete(markedList())};
document.getElementById('chkAll').onchange=function(){
  if(this.checked){
    (listEl._items||[]).forEach(function(e){marked[fullOf(e)]=e});
  }else{ clearMarks(); }
  render();
};
document.getElementById('sideRoot').onclick=function(){searchMode=false;go('/')};
document.getElementById('searchForm').onsubmit=function(ev){ev.preventDefault();runSearch(document.getElementById('searchBox').value)};
document.addEventListener('click',function(ev){if(!menu.contains(ev.target)) hideMenu()});
document.addEventListener('keydown',function(ev){
  if(ev.key==='Escape'){
    if(document.getElementById('dlgBg').classList.contains('open')){ev.preventDefault(); closeDlg(null); return}
    hideMenu();
    if(document.getElementById('modalBg').classList.contains('open')) closeModal();
    else if(document.getElementById('previewBar').classList.contains('on')) cancelAllPreview();
  }
  if(document.getElementById('dlgBg').classList.contains('open')) return;
  if(document.getElementById('loginBg').classList.contains('open')) return;
  if(ev.key==='F2'&&selected){ev.preventDefault();runAction('rename',selected)}
  if(ev.key==='Delete'&&selected){ev.preventDefault();runAction('delete',selected)}
  if(ev.ctrlKey&&(ev.key==='c'||ev.key==='C')&&selected){ev.preventDefault();runAction('copy',selected)}
  if(ev.ctrlKey&&(ev.key==='x'||ev.key==='X')&&selected){ev.preventDefault();runAction('cut',selected)}
  if(ev.ctrlKey&&(ev.key==='v'||ev.key==='V')){ev.preventDefault();runAction('paste', selected&&selected.dir?selected:{name:'',dir:true,path:path})}
  if(ev.key==='Enter'&&selected){ev.preventDefault(); if(selected.dir) go(fullOf(selected)); else showPreview(selected)}
});

(function(){
  var hp=window.WFM_HOME_PORT|0;
  var a=document.getElementById('homeLink');
  if(a && hp>0){
    a.href=location.protocol+'//'+location.hostname+':'+hp+'/';
    a.style.display='';
  }
})();

ensureAuth().then(function(){ return go('/'); }).then(function(){loadStatus()});
setInterval(function(){
  if(document.getElementById('loginBg').classList.contains('open')) return;
  loadStatus();
},8000);
})();
</script>
</body>
</html>
)HTML";
