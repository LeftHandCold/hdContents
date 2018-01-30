#include "hdtd.h"
#include <ctype.h>
//============ patch get first char
static unsigned patch_get_first_char(wchar_t *unicode) {
    char buf[4];
    wchar_t wc = unicode[0];
    //Is it a letter?
    if ((wc >= 'a' && wc <= 'z') || (wc >= 'A' && wc <= 'Z'))
        wc = toupper(unicode[0]);

    int size = wctomb(buf, wc);
    //int size=wctomb(buf,toupper(unicode[0]));
    if (size > 0) {
        return (unsigned char)buf[0];
    }
    else {
        return toupper(unicode[0]);
    }
}
//================ patch unicode to utf8 ==================
static int to_utf8(char *buf, wchar_t c, int buf_size) {
    int size = 0;
    //0x0000-0x007f
    if (c < 0x80) {
        size = 1;
        if (buf_size < size) {
            return -1;
        }
        buf[0] = c;

    }
    else if (c < 0x0800) {
        size = 1;
        if (buf_size < size) {
            return -1;
        }
        //0x0080-0x07ff
        //110X XXXX
        buf[0] = 0b11000000 + (c >> 6);
        //10XX XXXX
        buf[1] = 0b10000000 + (c & 0b00111111);
        size = 2;
    }
    else if (c < 0x10000) {
        size = 1;
        if (buf_size < size) {
            return -1;
        }
        //1110 XXXX
        buf[0] = 0b11100000 + (c >> 12);
        //10XX XXXX
        buf[1] = 0b10000000 + ((c >> 6) & 0b111111);
        //10XX XXXX
        buf[2] = 0b10000000 + (c & 0b111111);
        size = 3;
    }
    else {
        size = 1;
        if (buf_size < size) {
            return -1;
        }
        //1111 0XXX
        //10XX XXXX
        buf[0] = 0b11110000 + (c >> 18);
        //10XX XXXX
        buf[1] = 0b10000000 + ((c >> 12) & 0b111111);
        //10XX XXXX
        buf[2] = 0b10000000 + ((c >> 6) & 0b111111);
        //10XX XXXX
        buf[3] = 0b10000000 + (c & 0b111111);
        size = 4;
    }

    return size;
}
static int unicode_to_utf8(char *buf, wchar_t *unicode, int buf_size) {
    char *p = buf;
    int total_size = 0;
    //Used to store'\0'
    buf_size -= 1;
    int i;
    for (i = 0; unicode[i] != 0; i++) {
        int size = to_utf8(p, unicode[i], buf_size);
        if (size < 0) {
            break;
        }
        p += size;
        buf_size -= size;
        total_size += size;
    }
    buf[total_size] = '\0';
    return total_size;
}

int main() {
    
    hd_context *ctx;
    hd_document *doc;
    ctx = hd_new_context(NULL, HD_STORE_DEFAULT);
    if (!ctx)
    {
        fprintf(stderr, "cannot create hdContents context\n");
        return EXIT_FAILURE;
    }

    /* Register the default file types to handle. */
    hd_try(ctx)
        hd_register_document_handlers(ctx);
    hd_catch(ctx)
    {
        fprintf(stderr, "cannot register document handlers: %s\n", hd_caught_message(ctx));
        hd_drop_context(ctx);
        return EXIT_FAILURE;
    }

    /* Open the document. */
    hd_try(ctx)
        doc = hd_open_document(ctx, "F:\\f0711656.pdf");
    hd_catch(ctx) {
        fprintf(stderr, "cannot open document: %s\n", hd_caught_message(ctx));
        hd_drop_context(ctx);
        return EXIT_FAILURE;
    }

    hd_page *page = hd_load_page(ctx, doc, 0);
    char buf[512] = {0};

    hd_try(ctx)
    {
        hd_run_page_contents(ctx, page, buf);
        unsigned char filenameUtf8[128];
        memset(filenameUtf8, 0, 128);
        unicode_to_utf8(filenameUtf8, buf, 32);
        FILE *fp;

        fp=fopen("F:\\test.txt","a+");
        fprintf(fp,"%s",filenameUtf8);
        fclose(fp);
    }
    hd_catch(ctx)
    {
        fprintf(stderr, "cannot run page contents: %s\n", hd_caught_message(ctx));
        hd_drop_context(ctx);
        hd_drop_document(ctx, doc);
        return EXIT_FAILURE;
    }
    hd_drop_document(ctx, doc);
    hd_drop_context(ctx);
    printf("hd_new_context is end\n");
}