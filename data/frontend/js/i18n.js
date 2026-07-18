const UiLocales = ["en", "de"];
const UiResources = {};
let UiReverseEnglish = new Map();
let UiTranslationObserver;
const UiOriginalText = new WeakMap();
const UiOriginalAttributes = new WeakMap();

function getUiLanguage() {
    const saved = localStorage.getItem("ui-language");
    if (UiLocales.includes(saved)) return saved;
    return navigator.language && navigator.language.toLowerCase().startsWith("de") ? "de" : "en";
}

function flattenResource(source, prefix = "", result = {}) {
    Object.entries(source || {}).forEach(([name, value]) => {
        const key = prefix ? `${prefix}.${name}` : name;
        if (value && typeof value === "object" && !Array.isArray(value)) flattenResource(value, key, result);
        else result[key] = value;
    });
    return result;
}

function resolveTranslation(locale, key) {
    return key.split(".").reduce((value, part) => value && value[part], UiResources[locale]);
}

function translate(key, values = {}) {
    const resourceKey = resolveTranslation("en", key) !== undefined ? key : UiReverseEnglish.get(key);
    let result = resourceKey ? resolveTranslation(getUiLanguage(), resourceKey) : undefined;
    if (result === undefined && resourceKey) result = resolveTranslation("en", resourceKey);
    if (result === undefined) result = key;
    Object.entries(values).forEach(([name, value]) => result = result.replaceAll(`{${name}}`, value));
    return result;
}

function translateElement(element) {
    if (!element || element.nodeType !== Node.ELEMENT_NODE) return;
    const key = element.dataset.i18n;
    if (key) element.textContent = translate(key);
    const attributeKeys = {"placeholder":"i18nPlaceholder", "title":"i18nTitle", "aria-label":"i18nAriaLabel"};
    Object.entries(attributeKeys).forEach(([attribute, datasetKey]) => {
        const attributeKey = element.dataset[datasetKey];
        if (attributeKey) {
            element.setAttribute(attribute, translate(attributeKey));
            return;
        }
        if (!element.hasAttribute(attribute)) return;
        let originals = UiOriginalAttributes.get(element);
        if (!originals) { originals = {}; UiOriginalAttributes.set(element, originals); }
        if (!(attribute in originals)) originals[attribute] = element.getAttribute(attribute);
        element.setAttribute(attribute, translate(originals[attribute]));
    });
    element.querySelectorAll("[data-i18n], [data-i18n-placeholder], [data-i18n-title], [data-i18n-aria-label]").forEach(child => {
        if (child !== element) translateElement(child);
    });
}

function translateLegacyText(root) {
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);
    nodes.forEach(node => {
        if (!node.parentElement || node.parentElement.closest("script, style, code, [data-i18n]")) return;
        if (!UiOriginalText.has(node)) UiOriginalText.set(node, node.nodeValue);
        const original = UiOriginalText.get(node);
        const trimmed = original.trim().replace(/\s+/g, " ");
        if (!UiReverseEnglish.has(trimmed)) { node.nodeValue = original; return; }
        node.nodeValue = original.match(/^\s*/)[0] + translate(UiReverseEnglish.get(trimmed)) + original.match(/\s*$/)[0];
    });
}

function applyTranslations(root = document.documentElement) {
    document.documentElement.lang = getUiLanguage();
    translateElement(root);
    translateLegacyText(root);
    const selector = document.getElementById("ui-language");
    if (selector) selector.value = getUiLanguage();
}

function setUiLanguage(language) {
    localStorage.setItem("ui-language", UiLocales.includes(language) ? language : "en");
    applyTranslations();
    document.dispatchEvent(new CustomEvent("ui-language-changed"));
}

function getUiTheme() {
    const saved = localStorage.getItem("ui-theme");
    if (saved === "dark" || saved === "light") return saved;
    return window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}
function applyUiTheme(theme = getUiTheme()) {
    document.documentElement.setAttribute("data-theme", theme);
    const toggle = document.getElementById("ui-dark-mode");
    if (toggle) toggle.checked = theme === "dark";
}
function setUiTheme(dark) {
    const theme = dark ? "dark" : "light";
    localStorage.setItem("ui-theme", theme);
    applyUiTheme(theme);
}

const i18nReady = Promise.all(UiLocales.map(async locale => {
    const response = await fetch(`/frontend/i18n/${locale}.json`);
    if (!response.ok) throw new Error(`Unable to load ${locale} translations (${response.status}).`);
    UiResources[locale] = await response.json();
})).then(() => {
    UiReverseEnglish = new Map(Object.entries(flattenResource(UiResources.en)).map(([key, value]) => [value, key]));
    applyTranslations();
    UiTranslationObserver = new MutationObserver(mutations => mutations.forEach(mutation => mutation.addedNodes.forEach(node => {
        if (node.nodeType === Node.ELEMENT_NODE) { translateElement(node); translateLegacyText(node); }
    })));
    UiTranslationObserver.observe(document.documentElement, {childList: true, subtree: true});
}).catch(error => console.error("i18n initialization failed:", error));

applyUiTheme();
