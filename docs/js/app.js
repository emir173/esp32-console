// ===================== SCHEMATIC PINOUT DATA + RENDER =====================
const pins = [
  { id:'tft',   x:110, y:40,  label:'TFT ST7735',  detail:'MOSI=11 · SCLK=12 · CS=15 · DC=41' },
  { id:'sd',    x:650, y:40,  label:'SD KART',      detail:'MOSI=11 · MISO=42 · SCK=12 · CS=10' },
  { id:'oled',  x:110, y:280, label:'OLED I²C',     detail:'SDA=8 · SCL=9' },
  { id:'joy',   x:650, y:280, label:'JOYSTICK',     detail:'VRX=GPIO1 · VRY=GPIO2 · SW=GPIO18' },
  { id:'btnab', x:160, y:160, label:'BUTON A/B',    detail:'BTN_A=GPIO3 · BTN_B=GPIO21' },
  { id:'btncd', x:600, y:160, label:'BUTON C/D',    detail:'BTN_C=GPIO4 · BTN_D=GPIO6' },
  { id:'buzz',  x:380, y:280, label:'BUZZER',       detail:'GPIO16' },
];

const linesG = document.getElementById('schemLines');
const nodesG = document.getElementById('schemNodes');
const readout = document.getElementById('pinReadout');
const cx = 380, cy = 160;

pins.forEach(p => {
  // connecting line
  const line = document.createElementNS('http://www.w3.org/2000/svg','path');
  const midX = (p.x + cx) / 2;
  line.setAttribute('d', `M ${p.x} ${p.y} Q ${midX} ${p.y} ${cx} ${cy}`);
  line.setAttribute('class', 'schem-line');
  linesG.appendChild(line);

  // node group
  const g = document.createElementNS('http://www.w3.org/2000/svg','g');
  g.setAttribute('class', 'schem-node');
  g.setAttribute('tabindex', '0');
  g.setAttribute('role', 'button');
  g.setAttribute('aria-label', p.label);

  const circle = document.createElementNS('http://www.w3.org/2000/svg','circle');
  circle.setAttribute('cx', p.x);
  circle.setAttribute('cy', p.y);
  circle.setAttribute('r', 7);
  g.appendChild(circle);

  const text = document.createElementNS('http://www.w3.org/2000/svg','text');
  text.setAttribute('x', p.x);
  text.setAttribute('y', p.y > cy ? p.y + 24 : p.y - 14);
  text.setAttribute('text-anchor', 'middle');
  text.textContent = p.label;
  g.appendChild(text);

  nodesG.appendChild(g);

  function activate() {
    document.querySelectorAll('.schem-node').forEach(n => n.classList.remove('active'));
    document.querySelectorAll('.schem-line').forEach(l => l.classList.remove('active'));
    g.classList.add('active');
    line.classList.add('active');
    readout.innerHTML = `<strong style="color:var(--trace)">${p.label}</strong><span class="pin-detail">${p.detail}</span>`;
  }

  g.addEventListener('mouseenter', activate);
  g.addEventListener('click', activate);
  g.addEventListener('focus', activate);
});

// 2. SCROLL REVEAL
const observerOptions = {
  root: null,
  rootMargin: '0px',
  threshold: 0.1
};

const observer = new IntersectionObserver((entries, observer) => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      entry.target.classList.add('active');
      observer.unobserve(entry.target);
    }
  });
}, observerOptions);

document.querySelectorAll('.reveal').forEach(el => {
  observer.observe(el);
});

// 3. CODE TYPING EFFECT
const codePres = document.querySelectorAll('.code-window pre');

codePres.forEach(codePre => {
  const originalHTML = codePre.innerHTML;
  codePre.innerHTML = '';
  codePre.classList.add('code-typing-container');

  const codeObserver = new IntersectionObserver((entries) => {
    if (entries[0].isIntersecting) {
      codeObserver.disconnect();
      codePre.classList.add('active');
      
      let i = 0;
      let isTag = false;
      let currentHTML = '';
      
      const cursor = document.createElement('span');
      cursor.className = 'type-cursor';
      
      const typeInterval = setInterval(() => {
        if (i >= originalHTML.length) {
          clearInterval(typeInterval);
          cursor.remove();
          return;
        }
        
        const char = originalHTML[i];
        currentHTML += char;
        
        if (char === '<') isTag = true;
        if (char === '>') isTag = false;
        
        if (!isTag) {
          codePre.innerHTML = currentHTML;
          codePre.appendChild(cursor);
        }
        
        i++;
      }, 8);
    }
  }, { threshold: 0.5 });

  codeObserver.observe(codePre);
});

let currentGalleryImgs = [];
let currentIndex = -1;

function openLightbox(imgEl) {
  if (imgEl && imgEl.closest) {
    const gallery = imgEl.closest('.game-gallery');
    currentGalleryImgs = Array.from(gallery.querySelectorAll('img.game-shot')).map(img => img.src);
    currentIndex = currentGalleryImgs.indexOf(imgEl.src);
  } else {
    currentGalleryImgs = [imgEl];
    currentIndex = 0;
  }
  
  updateLightboxImage();
  document.getElementById('lightbox').classList.add('active');
}

function updateLightboxImage() {
  if (currentIndex < 0 || currentIndex >= currentGalleryImgs.length) return;
  
  document.getElementById('lightbox-img').src = currentGalleryImgs[currentIndex];
  
  const prevBtn = document.getElementById('lb-prev');
  const nextBtn = document.getElementById('lb-next');
  
  if (prevBtn) prevBtn.style.display = currentIndex > 0 ? 'flex' : 'none';
  if (nextBtn) nextBtn.style.display = currentIndex < currentGalleryImgs.length - 1 ? 'flex' : 'none';
}

function navLightbox(dir, e) {
  if (e) e.stopPropagation();
  let newIdx = currentIndex + dir;
  if (newIdx >= 0 && newIdx < currentGalleryImgs.length) {
    currentIndex = newIdx;
    updateLightboxImage();
  }
}

function closeLightbox() {
  document.getElementById('lightbox').classList.remove('active');
}

document.addEventListener('keydown', (e) => {
  if (!document.getElementById('lightbox').classList.contains('active')) return;
  if (e.key === 'Escape') closeLightbox();
  if (e.key === 'ArrowLeft') navLightbox(-1);
  if (e.key === 'ArrowRight') navLightbox(1);
});


// ===================== DYNAMIC RENDER =====================
function renderDynamicItems(lang) {
  function renderItem(item) {
    const tagsHtml = item.tags.map(t => `<span class="${t.class}">${t.text}</span>`).join('');
    const galleryHtml = item.gallery.map((img, idx) => 
      `<div class="game-shot-wrap" onclick="openLightbox(this.querySelector('img'))">
         <img src="${img}" class="game-shot" loading="lazy" decoding="async" alt="${item.name} screenshot ${idx + 1}">
       </div>`
    ).join('');
    
    return `
      <div class="game-row reveal">
        <div class="game-idx">${item.id}</div>
        <div class="game-content">
          <div class="game-name">${item.name}</div>
          <div class="game-desc">${lang === 'tr' ? item.desc_tr : item.desc_en}</div>
          ${tagsHtml}
          <div class="gallery-wrap">
            ${['DOOM', 'DUNGEON'].includes(item.name.toUpperCase()) ? `<div class="gallery-nav left" onclick="this.parentElement.querySelector('.game-gallery').scrollBy({left: -212, behavior: 'smooth'})">❮</div>` : ''}
            <div class="game-gallery">
              ${galleryHtml}
            </div>
            ${['DOOM', 'DUNGEON'].includes(item.name.toUpperCase()) ? `<div class="gallery-nav right" onclick="this.parentElement.querySelector('.game-gallery').scrollBy({left: 212, behavior: 'smooth'})">❯</div>` : ''}
          </div>
        </div>
      </div>
    `;
  }

  const gamesContainer = document.getElementById('games-container');
  if (gamesContainer && typeof gamesData !== 'undefined') {
    const categories = {};
    gamesData.forEach(game => {
      const cat = game.category || 'Other';
      if (!categories[cat]) categories[cat] = [];
      categories[cat].push(game);
    });
    
    let html = '';
    for (const [cat, games] of Object.entries(categories)) {
      html += `<div class="category-header reveal" style="margin-top:40px; margin-bottom:20px; border-bottom:1px solid var(--line); padding-bottom:8px; font-family:var(--mono); color:var(--accent); font-size:14px; letter-spacing:1px;">[ ${cat.toUpperCase()} ]</div>`;
      html += games.map(renderItem).join('');
    }
    gamesContainer.innerHTML = html;
  }

  const appsContainer = document.getElementById('apps-container');
  if (appsContainer && typeof appsData !== 'undefined') {
    appsContainer.innerHTML = appsData.map(renderItem).join('');
  }

  // Re-initialize scroll reveal for new elements
  if (typeof observer !== 'undefined') {
    document.querySelectorAll('.reveal').forEach(el => observer.observe(el));
  }
}

window.addEventListener('i18nReady', (e) => {
  renderDynamicItems(e.detail.lang);
});
