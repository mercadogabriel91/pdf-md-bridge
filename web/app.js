let worker;
let nextId = 0;
const pending = new Map();

function getWorker() {
  if (!worker) {
    worker = new Worker(new URL("./converter-worker.js", import.meta.url), {
      type: "module",
    });

    worker.onmessage = (event) => {
      const { type, id, markdown, message } = event.data;
      const job = pending.get(id);
      if (!job) {
        return;
      }
      pending.delete(id);

      if (type === "result") {
        job.resolve(markdown);
      } else if (type === "error") {
        job.reject(new Error(message));
      }
    };

    worker.onerror = (event) => {
      for (const [, job] of pending) {
        job.reject(new Error(event.message || "Worker failed"));
      }
      pending.clear();
      worker = undefined;
    };
  }
  return worker;
}

function toTransferableBuffer(bytes) {
  if (bytes.byteOffset === 0 && bytes.byteLength === bytes.buffer.byteLength) {
    return bytes.buffer;
  }
  return bytes.slice().buffer;
}

export function convertPdfToMarkdown(bytes) {
  const id = ++nextId;

  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });

    const buffer = toTransferableBuffer(bytes);
    getWorker().postMessage({ type: "convert", id, buffer }, [buffer]);
  });
}
