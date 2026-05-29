#!/usr/bin/env python3
"""
PDF to Markdown converter.

Extracts text and renders page images from a PDF, producing a .md file
that an AI agent can read — including visual elements like diagrams and flowcharts.

Dependencies:
    pip install pdfplumber pdf2image Pillow

System dependency:
    brew install poppler  (macOS)

Usage:
    python pdf_to_md.py input.pdf
    python pdf_to_md.py input.pdf --output custom_name.md
    python pdf_to_md.py input.pdf --dpi 200
    python pdf_to_md.py input.pdf --no-images   # text-only, skip image rendering
"""

import argparse
import os
import sys
from pathlib import Path


def check_dependencies():
    """Verify required packages are installed."""
    missing = []
    try:
        import pdfplumber  # noqa: F401
    except ImportError:
        missing.append("pdfplumber")
    try:
        from pdf2image import convert_from_path  # noqa: F401
    except ImportError:
        missing.append("pdf2image")
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        missing.append("Pillow")

    if missing:
        print(f"Missing packages: {', '.join(missing)}")
        print(f"Install with:  pip install {' '.join(missing)}")
        sys.exit(1)


def extract_text_from_page(page) -> str:
    """Extract text from a single pdfplumber page."""
    text = page.extract_text()
    if text:
        return text.strip()

    # Fallback: try extracting from tables
    tables = page.extract_tables()
    if tables:
        lines = []
        for table in tables:
            for row in table:
                cells = [str(c) if c else "" for c in row]
                lines.append(" | ".join(cells))
            lines.append("")
        return "\n".join(lines).strip()

    return ""


def render_page_images(pdf_path: str, output_dir: str, dpi: int) -> list[str]:
    """Render each PDF page as a PNG image. Returns list of image filenames."""
    from pdf2image import convert_from_path

    os.makedirs(output_dir, exist_ok=True)
    images = convert_from_path(pdf_path, dpi=dpi)
    filenames = []
    for i, img in enumerate(images, start=1):
        fname = f"page_{i:03d}.png"
        img.save(os.path.join(output_dir, fname), "PNG")
        filenames.append(fname)
        print(f"  Rendered page {i}/{len(images)}")
    return filenames


def build_markdown(
    pdf_path: str,
    page_texts: list[str],
    image_filenames: list[str] | None,
    images_dir: str | None,
) -> str:
    """Assemble the final markdown string."""
    pdf_name = Path(pdf_path).stem
    lines = [
        f"# {pdf_name}",
        "",
        f"> Auto-generated from `{Path(pdf_path).name}` — {len(page_texts)} pages",
        "",
        "---",
        "",
    ]

    for i, text in enumerate(page_texts, start=1):
        lines.append(f"## Page {i}")
        lines.append("")

        # Embed page image if available
        if image_filenames and images_dir:
            img_rel = f"{images_dir}/{image_filenames[i - 1]}"
            lines.append(f"![Page {i}]({img_rel})")
            lines.append("")

        if text:
            lines.append("### Extracted Text")
            lines.append("")
            lines.append(text)
        else:
            lines.append("*No extractable text on this page (image-only or scanned).*")

        lines.append("")
        lines.append("---")
        lines.append("")

    return "\n".join(lines)


def main():
    check_dependencies()

    parser = argparse.ArgumentParser(
        description="Convert a PDF to a Markdown file with page images."
    )
    parser.add_argument("pdf", help="Path to the input PDF file")
    parser.add_argument(
        "--output", "-o", help="Output .md file path (default: <pdf_name>.md)"
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=150,
        help="DPI for page image rendering (default: 150)",
    )
    parser.add_argument(
        "--no-images",
        action="store_true",
        help="Skip image rendering, produce text-only markdown",
    )
    args = parser.parse_args()

    pdf_path = args.pdf
    if not os.path.isfile(pdf_path):
        print(f"Error: file not found: {pdf_path}")
        sys.exit(1)

    pdf_stem = Path(pdf_path).stem
    output_md = args.output or f"{pdf_stem}.md"
    images_dir = f"{pdf_stem}_pages"

    # --- Extract text ---
    import pdfplumber

    print(f"Extracting text from {pdf_path}...")
    page_texts = []
    with pdfplumber.open(pdf_path) as pdf:
        for i, page in enumerate(pdf.pages, start=1):
            text = extract_text_from_page(page)
            page_texts.append(text)
            status = f"{len(text)} chars" if text else "no text"
            print(f"  Page {i}: {status}")

    # --- Render images ---
    image_filenames = None
    if not args.no_images:
        print(f"Rendering page images at {args.dpi} DPI...")
        try:
            image_filenames = render_page_images(pdf_path, images_dir, args.dpi)
        except Exception as e:
            print(f"Warning: could not render images ({e})")
            print("Continuing with text-only output. Install poppler: brew install poppler")
            image_filenames = None

    # --- Build markdown ---
    md_content = build_markdown(
        pdf_path,
        page_texts,
        image_filenames,
        images_dir if image_filenames else None,
    )

    with open(output_md, "w", encoding="utf-8") as f:
        f.write(md_content)

    print(f"\nDone! Output: {output_md}")
    if image_filenames:
        print(f"Page images: {images_dir}/")


if __name__ == "__main__":
    main()
