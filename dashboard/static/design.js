'use strict';
window.applyPopXAppearance = settings => {
    const design = window.POPX_DESIGN;
    if (!design) return;
    const values = {...design.defaults, ...settings};
    const colors = design.themes[values.theme] || design.themes.lagoon;
    for (const [key, value] of Object.entries(colors)) document.documentElement.style.setProperty(`--${key}`, value);
    document.documentElement.style.setProperty('--radius', `${design.radius}px`);
    document.documentElement.style.setProperty('--ui-scale', values.ui_scale);
    document.body.style.fontSize = `${15 * values.ui_scale}px`;
};
document.addEventListener('DOMContentLoaded', () => {
    window.applyPopXAppearance({});
    const nav = document.querySelector('aside nav');
    if (nav && !nav.querySelector('[href="/static/sandbox.html"]')) {
        const a = document.createElement('a'); a.href='/static/sandbox.html'; a.textContent='Sandbox e Learning Mixer'; nav.append(a);
    }
    const style=document.createElement('style');style.textContent=`nav a{display:block;color:var(--text);text-decoration:none;padding:9px 13px;border:1px solid var(--line);border-radius:var(--radius)}button,input,select{border-radius:var(--radius);background:var(--panel)}.primary{background:var(--accent)}#network{background:var(--bg)}`;document.head.append(style);
    let busy=false;
    const refresh=async()=>{if(busy||document.hidden)return;busy=true;try{const r=await fetch('/api/status');if(r.ok)window.applyPopXAppearance((await r.json()).appearance);}catch{}finally{busy=false;}};
    refresh();setInterval(refresh,3000);
});
