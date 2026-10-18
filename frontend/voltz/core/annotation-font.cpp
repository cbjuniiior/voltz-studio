#include "annotation-font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_OUTLINE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>

namespace voltz {

namespace fontdata {
// annotation-font-data.cpp, gerado pelo embed-fonts.cmake
extern const unsigned char kDmSansRegular[];
extern const size_t kDmSansRegularSize;
extern const unsigned char kDmSansBold[];
extern const size_t kDmSansBoldSize;
} // namespace fontdata

namespace {

constexpr double kMaxGlyphPixelSize = 2000.0;
constexpr double kMaxGlyphOrigin = 100000000.0;

int fontIndex(TextFont font)
{
	return font == TextFont::Regular ? 0 : 1;
}

class EmbeddedFonts;

// Medidas de uma das fontes: lidas uma vez ao abrir; o avanço pergunta à fonte (com o mutex).
class FaceMetrics : public GlyphMetrics {
public:
	int unitsPerEm() const override { return units_; }
	int ascender() const override { return ascender_; }
	int descender() const override { return descender_; }
	int lineHeight() const override { return lineHeight_; }
	int advance(char32_t codepoint) const override;

	void set(EmbeddedFonts *owner, int index, int units, int ascender, int descender, int lineHeight)
	{
		owner_ = owner;
		index_ = index;
		units_ = units;
		ascender_ = ascender;
		descender_ = descender;
		lineHeight_ = lineHeight;
	}

private:
	EmbeddedFonts *owner_ = nullptr;
	int index_ = 0;
	int units_ = 0; // 0 = a fonte não abriu: layout vazio
	int ascender_ = 0;
	int descender_ = 0;
	int lineHeight_ = 0;
};

// Biblioteca FreeType e as duas fontes, abertas na primeira vez que alguém precisa (estático da função: thread-safe) e
// fechadas na saída do binário.
class EmbeddedFonts {
public:
	EmbeddedFonts()
	{
		if (FT_Init_FreeType(&library_) != 0) {
			library_ = nullptr;
			return;
		}
		open(0, fontdata::kDmSansRegular, fontdata::kDmSansRegularSize);
		open(1, fontdata::kDmSansBold, fontdata::kDmSansBoldSize);
	}

	~EmbeddedFonts()
	{
		for (FT_Face &face : faces_) {
			if (face) {
				FT_Done_Face(face);
				face = nullptr;
			}
		}
		if (library_) {
			FT_Done_FreeType(library_);
		}
	}

	EmbeddedFonts(const EmbeddedFonts &) = delete;
	EmbeddedFonts &operator=(const EmbeddedFonts &) = delete;

	bool available(int index) const { return faces_[index] != nullptr; }
	const FaceMetrics &metrics(int index) const { return metrics_[index]; }

	int advance(int index, char32_t codepoint)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		FT_Face face = faces_[index];
		if (!face) {
			return 0;
		}
		FT_Fixed advance = 0;
		const FT_UInt glyph = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
		if (FT_Get_Advance(face, glyph, FT_LOAD_NO_SCALE, &advance) != 0) {
			return 0;
		}
		return static_cast<int>(advance);
	}

	bool render(int index, char32_t codepoint, double pixelSize, double originX, double baseline, GlyphBitmap &out)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		FT_Face face = faces_[index];
		if (!face || !(pixelSize > 0.0) || !std::isfinite(originX) || !std::isfinite(baseline)) {
			return false;
		}
		const double size = std::min(pixelSize, kMaxGlyphPixelSize);
		const auto charSize = static_cast<FT_F26Dot6>(std::max(1L, std::lround(size * 64.0)));
		if (FT_Set_Char_Size(face, 0, charSize, 72, 72) != 0) {
			return false;
		}
		const FT_UInt glyph = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
		if (FT_Load_Glyph(face, glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
			return false;
		}
		FT_GlyphSlot slot = face->glyph;
		if (slot->format != FT_GLYPH_FORMAT_OUTLINE || slot->outline.n_points == 0) {
			return false; /* espaço e afins: nada a desenhar */
		}
		/* a parte inteira vai para a posição do bitmap; a fração desloca o contorno (y do FreeType cresce para cima) */
		const double baseX = std::floor(std::clamp(originX, -kMaxGlyphOrigin, kMaxGlyphOrigin));
		const double baseY = std::floor(std::clamp(baseline, -kMaxGlyphOrigin, kMaxGlyphOrigin));
		const auto shiftX = static_cast<FT_Pos>(std::lround((originX - baseX) * 64.0));
		const auto shiftY = static_cast<FT_Pos>(std::lround((baseline - baseY) * 64.0));
		FT_Outline_Translate(&slot->outline, shiftX, -shiftY);
		if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
			return false;
		}
		const FT_Bitmap &bitmap = slot->bitmap;
		if (bitmap.width == 0 || bitmap.rows == 0 || bitmap.pixel_mode != FT_PIXEL_MODE_GRAY ||
		    bitmap.pitch < 0) {
			return false;
		}
		out.left = static_cast<int>(baseX) + slot->bitmap_left;
		out.top = static_cast<int>(baseY) - slot->bitmap_top;
		out.width = static_cast<int>(bitmap.width);
		out.height = static_cast<int>(bitmap.rows);
		out.coverage.resize(static_cast<size_t>(out.width) * static_cast<size_t>(out.height));
		for (int y = 0; y < out.height; y++) {
			std::memcpy(out.coverage.data() + static_cast<size_t>(y) * static_cast<size_t>(out.width),
				    bitmap.buffer + static_cast<size_t>(y) * static_cast<size_t>(bitmap.pitch),
				    static_cast<size_t>(out.width));
		}
		return true;
	}

private:
	void open(int index, const unsigned char *data, size_t size)
	{
		FT_Face face = nullptr;
		if (FT_New_Memory_Face(library_, data, static_cast<FT_Long>(size), 0, &face) != 0) {
			return;
		}
		if (!FT_IS_SCALABLE(face) || face->units_per_EM == 0) {
			FT_Done_Face(face);
			return;
		}
		faces_[index] = face;
		metrics_[index].set(this, index, face->units_per_EM, face->ascender, face->descender, face->height);
	}

	std::mutex mutex_;
	FT_Library library_ = nullptr;
	FT_Face faces_[2] = {nullptr, nullptr};
	FaceMetrics metrics_[2];
};

EmbeddedFonts &embeddedFonts()
{
	static EmbeddedFonts fonts;
	return fonts;
}

int FaceMetrics::advance(char32_t codepoint) const
{
	return owner_ ? owner_->advance(index_, codepoint) : 0;
}

} // namespace

bool embeddedFontAvailable(TextFont font)
{
	return embeddedFonts().available(fontIndex(font));
}

const GlyphMetrics &embeddedFontMetrics(TextFont font)
{
	return embeddedFonts().metrics(fontIndex(font));
}

bool renderGlyph(TextFont font, char32_t codepoint, double pixelSize, double originX, double baseline, GlyphBitmap &out)
{
	return embeddedFonts().render(fontIndex(font), codepoint, pixelSize, originX, baseline, out);
}

Stroke textCaretStroke(const Stroke &text)
{
	Stroke caret;
	caret.id = text.id;
	caret.tool = AnnotationTool::Pen;
	caret.color = text.color;
	if (text.tool != AnnotationTool::Text || text.points.empty()) {
		return caret;
	}
	const int size = clampTextSize(text.textSize);
	const TextLayout layout = layoutText(text.text, static_cast<double>(size), embeddedFontMetrics(text.textFont));
	if (layout.pixelSize <= 0.0) {
		return caret;
	}
	caret.width = clampStrokeWidth(std::max(2, size / 16));
	const StrokePoint anchor = text.points.front();
	const int x = anchor.x + static_cast<int>(std::lround(layout.caretX)) + caret.width / 2;
	const int top = anchor.y + static_cast<int>(std::lround(layout.caretTop));
	const int bottom = anchor.y + static_cast<int>(std::lround(layout.caretBottom)) - 1;
	caret.points = {StrokePoint{x, top}, StrokePoint{x, std::max(top, bottom)}};
	return caret;
}

} // namespace voltz
