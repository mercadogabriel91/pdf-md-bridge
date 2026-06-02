import createPdfMdBridgeModule from "./pdf_md_bridge.js";

let modulePromise;

function getModule() {
  if (!modulePromise) {
    modulePromise = createPdfMdBridgeModule();
  }
  return modulePromise;
}

export async function convertPdfToMarkdown(bytes) {
  const mod = await getModule();
  const ptr = mod._malloc(bytes.length);
  if (ptr === 0) {
    throw new Error("WASM malloc failed");
  }

  try {
    mod.HEAPU8.set(bytes, ptr);
    const resultPtr = mod._convert(ptr, bytes.length);
    if (resultPtr === 0) {
      throw new Error("Conversion failed (invalid PDF or out of memory)");
    }

    try {
      return mod.UTF8ToString(resultPtr);
    } finally {
      mod._free_result(resultPtr);
    }
  } finally {
    mod._free(ptr);
  }
}
