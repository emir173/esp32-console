class I18nManager {
  constructor() {
    this.currentLang = localStorage.getItem('eos_lang') || 'en';
    this.translations = {};
    this.init();
  }

  async init() {
    this.loadTranslations(this.currentLang);
    this.applyTranslations();
    this.updateHtmlLang();
    this.updateSwitcherUI();
    
    // Notify the rest of the app (like app.js) that translations are ready
    window.dispatchEvent(new CustomEvent('i18nReady', { detail: { lang: this.currentLang } }));
  }

  loadTranslations(lang) {
    if (typeof i18nData !== 'undefined' && i18nData[lang]) {
      this.translations = i18nData[lang];
    } else {
      console.error('Translations not found in i18nData for lang:', lang);
    }
  }

  applyTranslations() {
    const elements = document.querySelectorAll('[data-i18n]');
    elements.forEach(el => {
      const keyStr = el.getAttribute('data-i18n');
      const keys = keyStr.split('.');
      let value = this.translations;
      
      for (const k of keys) {
        if (value && value[k] !== undefined) {
          value = value[k];
        } else {
          value = null;
          break;
        }
      }

      if (value) {
        if (value.includes('<')) {
          el.innerHTML = value;
        } else {
          el.textContent = value;
        }
      }
    });
  }

  updateHtmlLang() {
    document.documentElement.lang = this.currentLang;
  }

  updateSwitcherUI() {
    const btn = document.getElementById('lang-switcher');
    if (btn) {
      btn.innerHTML = this.currentLang === 'tr' ? '🇬🇧 EN' : '🇹🇷 TR';
    }
  }

  async setLanguage(lang) {
    if (lang === this.currentLang) return;
    this.currentLang = lang;
    localStorage.setItem('eos_lang', lang);
    await this.init();
  }

  toggleLanguage() {
    const newLang = this.currentLang === 'tr' ? 'en' : 'tr';
    this.setLanguage(newLang);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  window.i18n = new I18nManager();

  const langBtn = document.getElementById('lang-switcher');
  if (langBtn) {
    langBtn.addEventListener('click', (e) => {
      e.preventDefault();
      window.i18n.toggleLanguage();
    });
  }
});
