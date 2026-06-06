// Bridge between the React UI and the JUCE backend.
// Wraps the official JUCE 8 WebView frontend (`juce-framework-frontend`)
// in a small typed API the rest of the app consumes.
//
// The `juce-framework-frontend` import is resolved by each consumer's
// vite.config.ts alias (the JUCE submodule path varies per target).

// @ts-expect-error — JUCE frontend ships as plain JS; aliased via consumer vite.config.ts.
import * as Juce from "juce-framework-frontend";

type Listener<T> = (data: T) => void;

declare global {
  interface Window {
    __JUCE__?: {
      backend: {
        addEventListener: (id: string, cb: (data: any) => void) => unknown;
        removeEventListener: (handle: unknown) => void;
        emitEvent: (id: string, data: any) => void;
      };
      initialisationData?: {
        __juce__functions?: string[];
        [k: string]: unknown;
      };
    };
  }
}

export type SliderState = {
  getScaledValue: () => number;
  getNormalisedValue: () => number;
  setNormalisedValue: (v: number) => void;
  sliderDragStarted: () => void;
  sliderDragEnded: () => void;
  valueChangedEvent: { addListener: (fn: () => void) => number; removeListener: (id: number) => void };
  propertiesChangedEvent: { addListener: (fn: () => void) => number; removeListener: (id: number) => void };
  properties: { start: number; end: number; numSteps: number };
};

export type ComboBoxState = {
  getChoiceIndex: () => number;
  setChoiceIndex: (i: number) => void;
  valueChangedEvent: { addListener: (fn: () => void) => number; removeListener: (id: number) => void };
  propertiesChangedEvent: { addListener: (fn: () => void) => number; removeListener: (id: number) => void };
  properties: { choices: string[] };
};

export type ToggleState = {
  getValue: () => boolean;
  setValue: (v: boolean) => void;
  valueChangedEvent: { addListener: (fn: () => void) => number; removeListener: (id: number) => void };
  propertiesChangedEvent: { addListener: (fn: () => void) => number; removeListener: (id: number) => void };
};

export function getSliderState(name: string): SliderState {
  return Juce.getSliderState(name) as SliderState;
}

export function getComboBoxState(name: string): ComboBoxState {
  return Juce.getComboBoxState(name) as ComboBoxState;
}

export function getToggleState(name: string): ToggleState {
  return Juce.getToggleState(name) as ToggleState;
}

export async function callNative<T = unknown>(
  name: string,
  ...args: unknown[]
): Promise<T | undefined> {
  if (typeof window === "undefined" || window.__JUCE__?.initialisationData?.__juce__functions === undefined) {
    console.warn(`Native function ${name} not available (dev mode?)`);
    return undefined;
  }
  const fn = Juce.getNativeFunction(name) as (...a: unknown[]) => Promise<T>;
  return fn(...args);
}

export function onBackendEvent<T = unknown>(
  id: string,
  cb: Listener<T>
): () => void {
  if (typeof window === "undefined" || !window.__JUCE__) {
    return () => {};
  }
  const handle = window.__JUCE__.backend.addEventListener(id, (data) => cb(data as T));
  return () => window.__JUCE__?.backend.removeEventListener(handle);
}
