// Capture the built Player React UI with deterministic example-backed bridge data.
// This fixture adapter reads only the simple, one-opcode-per-line example SFZs;
// it is not an SFZ parser or a replacement for the native Player integration tests.
// Serve the repo root at 127.0.0.1:18743. Point PLAYWRIGHT_MODULE to an installed
// Playwright index.mjs, or install Playwright where Node can resolve it.
import { readFile, mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
import assert from 'node:assert/strict';
const { chromium } = await import(process.env.PLAYWRIGHT_MODULE ?? 'playwright');
const root = fileURLToPath(new URL('../', import.meta.url));
const base = 'http://127.0.0.1:18743';
const browser = await chromium.launch({ headless: true, ...(process.env.PLAYWRIGHT_EXECUTABLE_PATH ? { executablePath: process.env.PLAYWRIGHT_EXECUTABLE_PATH } : {}) });
try {
  for (const directory of ['manifest-instrument', 'manifest-minimal']) {
    const manifest = JSON.parse(await readFile(resolve(root, 'examples', directory, 'instrument.json'), 'utf8'));
    const preset = manifest.presets[0];
    const sfz = await readFile(resolve(root, 'examples', directory, preset.sfz), 'utf8');
    const labels = Object.fromEntries([...sfz.matchAll(/^label_cc(\d+)=(.+)$/gm)].map(m => [m[1], m[2]]));
    const values = Object.fromEntries([...sfz.matchAll(/^set_cc(\d+)=(\d+)$/gm)].map(m => [m[1], Number(m[2]) / 127]));
    const controls = preset.presentation?.sections.flatMap(section => section.controls.map(c => ({
      number: c.target.number, label: c.label || labels[c.target.number] || `CC ${c.target.number}`,
      value: values[c.target.number] ?? 0, widget: c.widget ?? 'knob', section: section.id,
      sectionLabel: section.label || section.id,
    }))) ?? Object.entries(labels).map(([number, label]) => ({
      number: Number(number), label, value: values[number] ?? 0, widget: 'knob', section: '', sectionLabel: '',
    }));
    const presentation = {
      instrumentName: manifest.instrument.name, presetName: preset.name, controls, diagnostics: [],
      accent: preset.presentation?.accent ?? '',
      artworkUrl: preset.artwork ? `${base}/examples/${directory}/${preset.artwork.controlsBackground}` : '',
    };
    const page = await browser.newPage({ viewport: { width: 1080, height: 780 }, deviceScaleFactor: 1 });
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(presentation => {
      window.__JUCE__ = {
        initialisationData: {
          __juce__platform: [], __juce__functions: ['getInstrumentPresentation', 'getStatus', 'getAppInfo', 'getRecentFiles', 'getKeyLabels'],
          __juce__registeredGlobalEventIds: [], __juce__sliders: ['gainDb', 'polyphony'], __juce__toggles: [], __juce__comboBoxes: [],
        },
        postMessage(text) {
          const { eventId, payload } = JSON.parse(text);
          const emit = (id, result) => queueMicrotask(() => window.__JUCE__.backend.emitByBackend(id, JSON.stringify(result)));
          if (eventId.startsWith('__juce__slider') && payload.eventType === 'requestInitialUpdate') {
            const gain = eventId === '__juce__slidergainDb';
            emit(eventId, { eventType: 'propertiesChanged', start: gain ? -60 : 1, end: gain ? 12 : 256,
              skew: 1, interval: gain ? 0.1 : 1, numSteps: gain ? 721 : 256, name: gain ? 'Gain' : 'Voices', label: '', parameterIndex: gain ? 0 : 1 });
            emit(eventId, { eventType: 'valueChanged', value: gain ? 0 : 16 });
          }
          if (eventId !== '__juce__invoke') return;
          const result = payload.name === 'getInstrumentPresentation' ? presentation
            : payload.name === 'getStatus' ? { ok: true, fileName: 'instrument.sfz', bundlePatchName: presentation.presetName, numRegions: 1, numPreloadedSamples: 0 }
            : payload.name === 'getAppInfo' ? { productName: 'Sfizioso Player', version: '' }
            : payload.name === 'getKeyLabels' ? { labels: [], used: [] } : [];
          emit('__juce__complete', { promiseId: payload.resultId, result });
        },
      };
    }, presentation);
    await page.goto(`${base}/src/player/ui/dist/index.html`);
    await page.getByText(presentation.instrumentName, { exact: true }).waitFor();
    await page.evaluate(async url => {
      await document.fonts.ready;
      if (url) { const image = new Image(); image.src = url; await image.decode(); }
    }, presentation.artworkUrl);
    assert.equal(await page.locator('.cc-section').count(), preset.presentation?.sections.length ?? 1);
    assert.deepEqual(errors, []);
    await mkdir(resolve(root, 'docs/assets'), { recursive: true });
    await page.screenshot({ path: resolve(root, `docs/assets/${directory}.png`), animations: 'disabled' });
    console.log(`Captured ${directory}`);
    await page.close();
  }
} finally { await browser.close(); }
