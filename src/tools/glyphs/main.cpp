#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <windows.h>

const char *LANG_ID;
const char *FONT_FACE;
const char *SOURCE_FILE;
const char *GLYPHS_FILE;
int BASE_LINE = 0;
#define FONT_HEIGHT  16

struct Color32 {
    char r, g, b, a;
};

struct Glyph {
    wchar_t value;
    int     count;
    int     x, y, w, h;

    static int cmp(const Glyph &a, const Glyph &b) {
        if (a.count > b.count) return -1;
        if (a.count < b.count) return +1;
        //if (a.value < b.value) return -1;
        //if (a.value > b.value) return +1;
        return 0;
    }
};

template <class T>
inline void swap(T &a, T &b) {
    T tmp = a;
    a = b;
    b = tmp;
}

template <class T>
void qsort(T* v, int L, int R) {
    int i = L;
    int j = R;
    const T m = v[(L + R) / 2];

    while (i <= j) {
        while (T::cmp(v[i], m) < 0) i++;
        while (T::cmp(m, v[j]) < 0) j--;

        if (i <= j)
            swap(v[i++], v[j--]);
    }

    if (L < j) qsort(v, L, j);
    if (i < R) qsort(v, i, R);
}

template <class T>
void sort(T *items, int count) {
    if (count)
        qsort(items, 0, count - 1);
}

void SaveBMP(const char *name, const char *data32, int width, int height) {
    BITMAPFILEHEADER fhdr;
    BITMAPINFOHEADER ihdr;

    memset(&fhdr, 0, sizeof(fhdr));
    memset(&ihdr, 0, sizeof(ihdr));

    ihdr.biSize         = sizeof(ihdr);
    ihdr.biWidth        = width;
    ihdr.biHeight       = height;
    ihdr.biPlanes       = 1;
    ihdr.biBitCount     = 1;
    ihdr.biSizeImage    = width * height / 8;

    fhdr.bfType         = 0x4D42;
    fhdr.bfOffBits      = sizeof(fhdr) + ihdr.biSize + sizeof(RGBQUAD) * 2;
    fhdr.bfSize         = fhdr.bfOffBits + ihdr.biSizeImage;

    char *data = new char[width * height / 8];

    char *dst = data;
    Color32 *src = (Color32*)data32;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width / 8; i++) {
            *dst = 0;
            for (int k = 0; k < 8; k++) {
                *dst |= (((src++)->r != 0) ? 1 : 0) << (7 - k);
            }
            dst++;
        }
    }

    char buf[256];
    strcpy(buf, name);
    strcat(buf, ".bmp");

    FILE *f = fopen(buf, "wb");
    if (f) {
        RGBQUAD bl = {0x00, 0x00, 0x00, 0x00}; // black color
        RGBQUAD wh = {0xFF, 0xFF, 0xFF, 0x00}; // white color

        fwrite( &fhdr, sizeof(fhdr), 1, f);
        fwrite( &ihdr, sizeof(ihdr), 1, f);
        fwrite( &bl,   sizeof(bl),   1, f);
        fwrite( &wh,   sizeof(wh),   1, f);

        fwrite(data, ihdr.biSizeImage, 1, f);
        fclose(f);
    }

    delete[] data;
}

int getGlyphIndex(Glyph *glyphs, int count, wchar_t c) {
    for (int i = 0; i < count; i++)
        if (glyphs[i].value == c) {
        // hack to remove NULL terminator
            i++;
            if (i > 255) i++;
            i += 256;
            return i;
        }
    return -1;
}

void collectGlyphs(Glyph *&glyphs, int &count, const wchar_t *wstr, int wlen, int &start, int &end) {
    count = 0;
    glyphs = new Glyph[wlen];
    memset(glyphs, 0, wlen * sizeof(Glyph));

    wchar_t head[5]; // #if 0
    memset(head, 0, sizeof(head));

    start = end = -1;
    bool flag = false;
    for (int i = 0; i < wlen; i++) {
        if (flag) {
            if (wstr[i] == '#') { // #endif
                end = i;
                break;
            }

            if (wstr[i] <= 255) continue;
            bool exists = false;
            for (int j = 0; j < count; j++) {
                if (glyphs[j].value == wstr[i]) {
                    glyphs[j].count++;
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                glyphs[count].value = wstr[i];
                glyphs[count].x = glyphs[count].y = 0;
                glyphs[count].w = glyphs[count].h = 16;
                glyphs[count].count = 1;
                count++;
            }

        } else {
            for (int i = 0; i < sizeof(head) / sizeof(head[0]) - 1; i++)
                head[i] = head[i + 1];
            head[sizeof(head) / sizeof(head[0]) - 1] = wstr[i];

            if (memcmp(head, u"#if 0", sizeof(head)) == 0) {
                start = i + 1;
                flag = true;
            }
        }
    }

    if (start == -1 || end == -1)
        return;

    sort(glyphs, count);
}

wchar_t *encodeString(Glyph *&glyphs, int &count, const wchar_t *wstr, int wlen, int start, int end, int &nlen) {
    if (start >= end)
        return NULL;

    wchar_t *nstr = new wchar_t[wlen * 8];
    nlen = end + 6;
    memcpy(nstr, wstr, nlen * 2);

    {
        char buf[4 * 1024];
        sprintf(buf, "\r\n\r\n#define %s_GLYPH_COUNT %d\r\n#define %s_GLYPH_BASE %d\r\nconst uint8 %s_GLYPH_WIDTH[] = {", LANG_ID, count, LANG_ID, BASE_LINE, LANG_ID);

        for (int i = 0; i < count; i++) {
            if (i % 16 == 0) {
                strcat(buf, "\r\n    ");
            }
            char num[32];
            sprintf(num, "%d, ", glyphs[i].w);
            strcat(buf, num);
        }
        strcat(buf, "};\r\n");

        for (int i = 0; i < int(strlen(buf)); i++)
            nstr[nlen++] = buf[i];
    }

    bool seq = false;

    for (int i = start; i < end; i++) {
        int index = getGlyphIndex(glyphs, count, wstr[i]);
        if (index == -1) {
            if (seq) {
                memcpy(nstr + nlen, u"\\xFF\\xFF", 10 * 2);
                nlen += 8;
                if (wstr[i] != '\"') {
                    memcpy(nstr + nlen, u"\"\"", 2 * 2);
                    nlen += 2;
                }
                seq = false;
            }
            nstr[nlen++] = wstr[i];
        } else {
            if (!seq) {
                memcpy(nstr + nlen, u"\\x11", 4 * 2);
                nlen += 4;
                seq = true;
            }

            char buf[5];
            sprintf(buf, "%04X", index);

            nstr[nlen++] = '\\';
            nstr[nlen++] = 'x';
            nstr[nlen++] = buf[0];
            nstr[nlen++] = buf[1];

            nstr[nlen++] = '\\';
            nstr[nlen++] = 'x';
            nstr[nlen++] = buf[2];
            nstr[nlen++] = buf[3];
        }
    }

    memcpy(nstr + nlen, u"\r\n#endif\r\n", 10 * 2);
    nlen += 10;

    return nstr;
}

wchar_t* loadStr(const char *fileName, int &wlen) {
    FILE *f = fopen(fileName, "rb");
    if (!f) {
        printf("can't open file %s\n", fileName);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *str = new char[size];
    fread(str, 1, size, f);
    fclose(f);

    wchar_t* wstr = new wchar_t[size];
    wlen = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, str, size, wstr, size);
    if (wlen == 0) {
        printf("can't convert utf-8 string to wide string\n");
        delete[] str;
        delete[] wstr;
        return NULL;
    }
    delete[] str;
    return wstr;
}

void saveStr(const char *fileName, wchar_t *wstr, int wlen) {
    if (!wstr) return;

    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, NULL, 0, NULL, NULL);
    
    if (len == 0) {
        printf("can't convert wide string to utf-8 string\n");
        return;
    }

    char *str = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, str, len, NULL, NULL);

    FILE *f = fopen(fileName, "wb");
    if (!f) {
        delete[] str;
        printf("can't open file %s\n", fileName);
        return;
    }
    fwrite(str, 1, len, f);
    fclose(f);

    delete[] str;
}

// GR Calibri "C:/Projects/OpenLara/src/lang/gr.h" "C:/Projects/OpenLara/src/lang/glyph_gr"
// JA "JF Dot Shinonome Gothic 16" "C:/Projects/OpenLara/src/lang/ja.h" "C:/Projects/OpenLara/src/lang/glyph_ja"
int main(int argc, char** argv) {
    if (argc != 5) {
        printf("glyphs font header bitmap\n");
        return 0;
    }

    LANG_ID      = argv[1];
    FONT_FACE    = argv[2];
    SOURCE_FILE  = argv[3];
    GLYPHS_FILE  = argv[4];

    // encode text
    int wlen;
    wchar_t *wstr = loadStr(SOURCE_FILE, wlen);

    if (wstr == NULL)
        return -1;

    int count;
    Glyph *glyphs;

    int start, end;
    collectGlyphs(glyphs, count, wstr, wlen, start, end);

    // generate image
    HDC hDC = GetDC(0);
    HFONT font = CreateFont(-FONT_HEIGHT, 0, 0, 0,
                            FW_NORMAL, // FW_SEMIBOLD,
                            FALSE,
                            FALSE, FALSE,
                            DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            NONANTIALIASED_QUALITY,
                            FF_DONTCARE,
                            FONT_FACE);

    if (!font) return -1;

    HDC mDC = CreateCompatibleDC(hDC);
    SelectObject(mDC, font);

    TEXTMETRICW tm;
    GetTextMetricsW(mDC, &tm);
    BASE_LINE = tm.tmDescent - 1;
    int charW = tm.tmMaxCharWidth;
    int charH = tm.tmHeight;

    if (strcmp(LANG_ID, "JA") == 0) {
        BASE_LINE = 0;
    }

    //if (charW > 16 || charH > 16) {
    //    printf("Glyphs too large %dx%d\n (16 required)\n", charW, charH);
    //    return -1;
    //}

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize      = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth     = 256;
    bi.bmiHeader.biHeight    = (count + 15) / 16 * 16;
    bi.bmiHeader.biPlanes    = 1;
    bi.bmiHeader.biBitCount  = 32;
    bi.bmiHeader.biSizeImage = bi.bmiHeader.biWidth * bi.bmiHeader.biHeight * 4;

    char *data = new char[bi.bmiHeader.biSizeImage];

    HBITMAP bmp = CreateCompatibleBitmap(hDC, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);

    SelectObject(mDC, bmp);
    SetBkMode(mDC, TRANSPARENT);
    SetTextColor(mDC, 0xFFFFFF);
    ReleaseDC(0, hDC);

    RECT rect = { 0, 0, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight };
    FillRect(mDC, &rect, HBRUSH(GetStockObject(BLACK_BRUSH)));

    for (int i = 0; i < count; i++) {
        wchar_t buf[2] = { glyphs[i].value, 0 };
        int dx = (i % 16) * 16;
        int dy = (i / 16) * 16;
        TextOutW(mDC, dx, dy - BASE_LINE, buf, 1);

        GdiFlush();
        GetDIBits(mDC, bmp, 0, bi.bmiHeader.biHeight, data, &bi, DIB_RGB_COLORS);

        Color32 *ptr = (Color32*)data + dx;

        glyphs[i].w = 0;
        for (int y = 0; y < 16; y++)
            for (int x = 15; x >= 0; x--) {
                if (ptr[(bi.bmiHeader.biHeight - (dy + y + 1)) * 256 + x].r != 0) {
                    glyphs[i].w = max(glyphs[i].w, x);
                    break;
                }
            }
        glyphs[i].w += 2;
    }


    SaveBMP(GLYPHS_FILE, data, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight);

    delete[] data;
    DeleteObject(bmp);
    DeleteObject(font);
    DeleteDC(mDC);

    int nlen;
    wchar_t *nstr = encodeString(glyphs, count, wstr, wlen, start, end, nlen);
    printf("unique characters count: %d\n", count);

    saveStr(SOURCE_FILE, nstr, nlen);
    delete[] wstr;
    delete[] nstr;
}