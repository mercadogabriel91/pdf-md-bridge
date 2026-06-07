import { convertPdfToMarkdown } from "./app.js";

const fileInput = document.getElementById("file");
const statusEl = document.getElementById("status");

function markdownFilename(pdfName) {
  const lower = pdfName.toLowerCase();
  if (lower.endsWith(".pdf")) {
    return pdfName.slice(0, -4) + ".md";
  }
  return pdfName + ".md";
}

function downloadMarkdown(markdown, filename) {
  const blob = new Blob([markdown], { type: "text/markdown;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  link.click();
  URL.revokeObjectURL(url);
}

fileInput.addEventListener("change", async () => {
  const file = fileInput.files?.[0];
  if (!file) return;

  const outName = markdownFilename(file.name);

  statusEl.textContent = `Converting ${file.name}…`;
  statusEl.className = "status";
  fileInput.disabled = true;

  try {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const markdown = await convertPdfToMarkdown(bytes);
    downloadMarkdown(markdown, outName);
    statusEl.textContent =
      `Downloaded ${outName} (${file.size.toLocaleString()} byte PDF)`;
  } catch (err) {
    statusEl.textContent = err instanceof Error ? err.message : String(err);
    statusEl.className = "status error";
  } finally {
    fileInput.disabled = false;
    fileInput.value = "";
  }
});
