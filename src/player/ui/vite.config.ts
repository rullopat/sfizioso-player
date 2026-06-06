import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { viteSingleFile } from "vite-plugin-singlefile";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));

// JUCE 8 ships its WebView frontend JS as plain ES modules under its module
// tree. We import it via this alias instead of duplicating the file — keeps
// us in sync with whatever JUCE submodule SHA the repo is pinned to.
const juceFrontend = path.resolve(
  here,
  "../../../external/JUCE/modules/juce_gui_extra/native/javascript/index.js"
);

// Shared design-system components live one level up at src/ui-shared/.
// Resolved at the consumer side so the same alias works for any target.
const shared = path.resolve(here, "../../ui-shared/src");

// React lives in this target's node_modules. Aliased explicitly so that
// shared files under src/ui-shared/ (which has no node_modules of its own)
// resolve through the consumer's install rather than walking up the FS tree.
const reactRoot = path.resolve(here, "node_modules/react");
const reactDomRoot = path.resolve(here, "node_modules/react-dom");

export default defineConfig({
  plugins: [react(), viteSingleFile()],
  resolve: {
    alias: {
      "juce-framework-frontend": juceFrontend,
      "@shared": shared,
      "react/jsx-runtime": path.join(reactRoot, "jsx-runtime.js"),
      "react/jsx-dev-runtime": path.join(reactRoot, "jsx-dev-runtime.js"),
      "react-dom/client": path.join(reactDomRoot, "client.js"),
      react: reactRoot,
      "react-dom": reactDomRoot,
    },
  },
  build: {
    target: "esnext",
    cssCodeSplit: false,
    assetsInlineLimit: 100_000_000,
    rollupOptions: {
      output: { inlineDynamicImports: true },
    },
  },
});
