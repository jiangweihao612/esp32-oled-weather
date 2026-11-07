# Merge auto generated Chinese glyph block into OLED_Data.cpp (idempotent).
import io
import os

here = os.path.dirname(os.path.abspath(__file__))
proj = os.path.dirname(here)
target = os.path.join(proj, "OLED_Data.cpp")
genfile = os.path.join(here, "cn_glyphs.gen")

with io.open(genfile, encoding="utf-8") as f:
    gen = f.read().replace("\r\n", "\n")
if not gen.startswith("\n"):
    gen = "\n" + gen
if not gen.endswith("\n"):
    gen += "\n"

with io.open(target, encoding="utf-8", newline="") as f:
    text = f.read()

BEGIN = "BEGIN AUTO GENERATED GLYPHS"
END = "END AUTO GENERATED GLYPHS"

if BEGIN in text:
    start = text.rindex("\n", 0, text.index(BEGIN)) + 1
    stop = text.index("\n", text.index(END)) + 1
    new_text = text[:start] + gen + text[stop:]
else:
    anchor = '\t"",'
    idx = text.index(anchor)
    start = text.rindex("\n", 0, idx) + 1
    new_text = text[:start] + gen + text[start:]

with io.open(target, "w", encoding="utf-8", newline="") as f:
    f.write(new_text)

count = new_text.count("GENERATED GLYPHS") // 2
print("merged glyph block, glyph entries:", new_text.count('\n\t"'))
