#include "format.h"

#include <stddef.h>
#include <stdint.h>

// clang-format off

#define FLAG_LEFT (1 << 0)
#define FLAG_SIGN (1 << 1)
#define FLAG_SPACE (1 << 2)
#define FLAG_ALTERNATIVE (1 << 3)
#define FLAG_ZERO (1 << 4)
#define FLAG_UPPERCASE (1 << 5)
#define FLAG_LEADING_ZERO (1 << 6)
#define FLAG_NO_PREFIX_ON_ZERO (1 << 7)

#define HASH(C) ((C) - 'a')
#define PUTCHAR(OUTFUNC, CH, COUNT) ({ (COUNT)++; OUTFUNC((CH)); })

enum {
    LNONE, Lh, Lhh, Ll, Lll, Lj, Lz, Lt, LL
};

enum {
    INT = 1, UINT,
    CHAR, UCHAR,
    SHORT, USHORT,
    LONG, ULONG, LLONG, ULLONG,
    PTR,
    DOUBLE, LDOUBLE,
    INTMAX, UINTMAX,
    SIZE, SSIZE,
    PTRDIFF, UPTRDIFF
};

static const uint8_t lookup[][26] = {
    { // NONE
        [HASH('c')] = INT, [HASH('s')] = PTR, [HASH('d')] = INT, [HASH('i')] = INT,
        [HASH('o')] = UINT, [HASH('x')] = UINT, [HASH('u')] = UINT,
        [HASH('f')] = DOUBLE, [HASH('e')] = DOUBLE, [HASH('a')] = DOUBLE, [HASH('g')] = DOUBLE,
        [HASH('n')] = PTR, [HASH('p')] = PTR
    },
    { // h
        [HASH('d')] = SHORT, [HASH('i')] = SHORT,
        [HASH('o')] = USHORT, [HASH('x')] = USHORT, [HASH('u')] = USHORT,
        [HASH('n')] = PTR
    },
    { // hh
        [HASH('d')] = CHAR, [HASH('i')] = CHAR,
        [HASH('o')] = UCHAR, [HASH('x')] = UCHAR, [HASH('u')] = UCHAR,
        [HASH('n')] = PTR
    },
    { // l
        [HASH('c')] = INT, [HASH('s')] = PTR, [HASH('d')] = LONG, [HASH('i')] = LONG,
        [HASH('o')] = ULONG, [HASH('x')] = ULONG, [HASH('u')] = ULONG,
        [HASH('f')] = DOUBLE, [HASH('e')] = DOUBLE, [HASH('a')] = DOUBLE, [HASH('g')] = DOUBLE,
        [HASH('n')] = PTR
    },
    { // ll
        [HASH('d')] = LLONG, [HASH('i')] = LLONG,
        [HASH('o')] = ULLONG, [HASH('x')] = ULLONG, [HASH('u')] = ULLONG,
        [HASH('n')] = PTR
    },
    { // j
        [HASH('d')] = INTMAX, [HASH('i')] = INTMAX,
        [HASH('o')] = UINTMAX, [HASH('x')] = UINTMAX, [HASH('u')] = UINTMAX,
        [HASH('n')] = PTR
    },
    { // z
        [HASH('d')] = SSIZE, [HASH('i')] = SSIZE,
        [HASH('o')] = SIZE, [HASH('x')] = SIZE, [HASH('u')] = SIZE,
        [HASH('n')] = PTR
    },
    { // t
        [HASH('d')] = PTRDIFF, [HASH('i')] = PTRDIFF,
        [HASH('o')] = UPTRDIFF, [HASH('x')] = UPTRDIFF, [HASH('u')] = UPTRDIFF,
        [HASH('n')] = PTR
    },
    { // L
        [HASH('f')] = LDOUBLE, [HASH('e')] = LDOUBLE, [HASH('a')] = LDOUBLE, [HASH('g')] = LDOUBLE
    }
};

typedef union {
    long double floatp;
    uintmax_t integer;
    void *pointer;
} arg_t;

static const char *prefixes = "\0000x\0000X\0+\0 ";

int format(format_writer_t writer, const char *format, va_list list) {
    char *fmt = (char *) format;
    char *fallback;

    uint64_t count = 0;

    uint8_t flags = 0;
    int precision = 0;
    int width = 0;
    uint8_t length_prefix = 0;

    bool negative = false;
    unsigned int radix = 0;
    arg_t value = {0};
    int prefix_offset = 0;

    lbl_normal:
    while(*fmt) {
        if(*fmt == '%') {
            flags = 0;
            precision = -1;
            width = 0;
            length_prefix = LNONE;
            fmt++;
            fallback = fmt;
            if(*fmt == '%') {
                PUTCHAR(writer, *fmt++, count);
                goto lbl_normal;
            }
            goto lbl_flags;
        }
        PUTCHAR(writer, *fmt++, count);
    }
    goto lbl_done;

    lbl_flags:
    while(*fmt) {
        switch(*fmt) {
            case '-': flags |= FLAG_LEFT; break;
            case '+': flags |= FLAG_SIGN; break;
            case ' ': flags |= FLAG_SPACE; break;
            case '#': flags |= FLAG_ALTERNATIVE; break;
            case '0': flags |= FLAG_ZERO; break;
            default:
                if(flags & FLAG_SIGN) flags &= ~FLAG_SPACE;
                if(flags & FLAG_LEFT) flags &= ~FLAG_ZERO;
                goto lbl_width;
        }
        fmt++;
    }
    goto lbl_done;

    lbl_width:
    if(*fmt == '*') {
        width = va_arg(list, int);
        if(width < 0) {
            width = -width;
            flags |= FLAG_LEFT;
        }
        fmt++;
        goto lbl_precision;
    }
    while(*fmt) {
        if(*fmt >= '0' && *fmt <= '9') {
            width *= 10;
            width += *fmt - '0';
        } else {
            goto lbl_precision;
        }
        fmt++;
    }
    goto lbl_done;

    lbl_precision:
    if(*fmt != '.') goto lbl_length;
    flags &= ~FLAG_ZERO;
    precision = 0;
    fmt++;
    if(*fmt == '*') {
        precision = va_arg(list, int);
        if(precision < 0) {
            precision = 1;
        }
        fmt++;
        goto lbl_length;
    }
    while(*fmt) {
        if(*fmt >= '0' && *fmt <= '9') {
            precision *= 10;
            precision += *fmt - '0';
        } else {
            goto lbl_length;
        }
        fmt++;
    }
    goto lbl_done;

    lbl_length:
    switch(*fmt) {
        case 'h': length_prefix = Lh; goto lbl_length_ext;
        case 'l': length_prefix = Ll; goto lbl_length_ext;
        case 'j': length_prefix = Lj; break;
        case 'z': length_prefix = Lz; break;
        case 't': length_prefix = Lt; break;
        case 'L': length_prefix = LL; break;
        default: goto lbl_modifiers;
    }
    fmt++;
    goto lbl_modifiers;

    lbl_length_ext:
    fmt++;
    switch(*fmt) {
        case 'h': length_prefix = Lhh; break;
        case 'l': length_prefix = Lll; break;
        default: goto lbl_modifiers;
    }
    fmt++;

    lbl_modifiers:
    uint8_t index = *fmt;
    if(index < 'a') index += ('a' - 'A');
    uint8_t size = lookup[length_prefix][HASH(index)];
    if(!size) size = lookup[LNONE][HASH(index)];
    if(!size) goto lbl_invalid;

    switch(size) {
        case INT:       value.integer = va_arg(list, signed int); break;
        case UINT:      value.integer = va_arg(list, unsigned int); break;
        case CHAR:      value.integer = (signed char) va_arg(list, signed int); break;
        case UCHAR:     value.integer = (unsigned char) va_arg(list, unsigned int); break;
        case SHORT:     value.integer = (signed short) va_arg(list, signed int); break;
        case USHORT:    value.integer = (unsigned short) va_arg(list, unsigned int); break;
        case LONG:      value.integer = va_arg(list, signed long); break;
        case ULONG:     value.integer = va_arg(list, unsigned long); break;
        case LLONG:     value.integer = va_arg(list, signed long long); break;
        case ULLONG:    value.integer = va_arg(list, unsigned long long); break;
        case PTR:       value.pointer = va_arg(list, void *); break;
        case INTMAX:    value.integer = va_arg(list, intmax_t); break;
        case UINTMAX:   value.integer = va_arg(list, uintmax_t); break;
        case SSIZE:
        case SIZE:      value.integer = va_arg(list, size_t); break;
        case UPTRDIFF:
        case PTRDIFF:   value.integer = va_arg(list, ptrdiff_t); break;
    }

    while(*fmt) {
        switch(*fmt) {
            case 'c':
                if(!(flags & FLAG_LEFT)) for(int i = 1; i < width; i++) PUTCHAR(writer, ' ', count);
                PUTCHAR(writer, (unsigned char) value.integer, count);
                if(flags & FLAG_LEFT) for(int i = 1; i < width; i++) PUTCHAR(writer, ' ', count);
                fmt++;
                goto lbl_normal;
            case 's':
                char *str = (char *) value.pointer;
                int length = 0;
                while(str[length]) ++length;
                if(precision >= 0 && precision < length) length = precision;
                if(!(flags & FLAG_LEFT)) for(int i = length; i < width; i++) PUTCHAR(writer, ' ', count);
                for(int i = 0; i < length; i++) PUTCHAR(writer, str[i], count);
                if(flags & FLAG_LEFT) for(int i = length; i < width; i++) PUTCHAR(writer, ' ', count);
                fmt++;
                goto lbl_normal;
            case 'X':
                flags |= FLAG_UPPERCASE;
                [[fallthrough]];
            case 'x':
                if(precision < 0) precision = 1;
                if(flags & FLAG_ALTERNATIVE) {
                    if(flags & FLAG_UPPERCASE) prefix_offset = 4; 
                    else prefix_offset = 1;
                } else prefix_offset = 0;
                radix = 16;
                negative = false;
                flags |= FLAG_NO_PREFIX_ON_ZERO;
                goto lbl_print_number;
            case 'u':
                if(precision < 0) precision = 1;
                prefix_offset = 0;
                radix = 10;
                negative = false;
                goto lbl_print_number;
            case 'o':
                if(precision < 0) precision = 1;
                prefix_offset = 0;
                radix = 8;
                negative = false;
                if(flags & FLAG_ALTERNATIVE) flags |= FLAG_LEADING_ZERO;
                flags |= FLAG_NO_PREFIX_ON_ZERO;
                goto lbl_print_number;
            case 'd':
            case 'i':
                if(precision < 0) precision = 1;
                if(flags & FLAG_SIGN) prefix_offset = 7;
                else if(flags & FLAG_SPACE) prefix_offset = 9;
                else prefix_offset = 0;
                radix = 10;
                goto lbl_print_signed_number;
            default: goto lbl_invalid;
        }
    }
    goto lbl_done;

    lbl_print_signed_number:
    negative = ((intmax_t) value.integer) < 0;
    if(negative) value.integer = -value.integer;

    lbl_print_number:
    fmt++;
    uint64_t pw = 1;
    int length = (precision || value.integer);
    while(value.integer / pw >= radix) {
        pw *= radix;
        length++;
    }
    int precision_pad = 0;
    if(precision > length) precision_pad = precision - length;
    if(precision_pad == 0 && (flags & FLAG_LEADING_ZERO) && (value.integer != 0 || !(flags & FLAG_NO_PREFIX_ON_ZERO))) precision_pad = 1;
    length += precision_pad;
    if(negative) length++;
    else if(!(flags & FLAG_NO_PREFIX_ON_ZERO) || value.integer != 0) for(int i = prefix_offset; prefixes[i]; i++) length++;

    if(!(flags & (FLAG_LEFT | FLAG_ZERO))) for(int i = length; i < width; i++) PUTCHAR(writer, ' ', count);
    if(!length) goto lbl_normal;
    if(negative) PUTCHAR(writer, '-', count);
    else if(!(flags & FLAG_NO_PREFIX_ON_ZERO) || value.integer != 0) while(prefixes[prefix_offset]) PUTCHAR(writer, prefixes[prefix_offset++], count);
    if((flags & (FLAG_ZERO | FLAG_LEFT)) == FLAG_ZERO) for(int i = length; i < width; i++) PUTCHAR(writer, '0', count);
    for(int i = 0; i < precision_pad; i++) PUTCHAR(writer, '0', count);
    while(pw != 0 && (precision || value.integer)) {
        uint8_t c = value.integer / pw;
        if(c >= 10) {
            PUTCHAR(writer, c - 10 + ((flags & FLAG_UPPERCASE) ? 'A' : 'a'), count);
        } else {
            PUTCHAR(writer, c + '0', count);
        }
        value.integer %= pw;
        pw /= radix;
    }
    if(flags & FLAG_LEFT) for(int i = length; i < width; i++) PUTCHAR(writer, ' ', count);
    goto lbl_normal;

    lbl_invalid:
        PUTCHAR(writer, '%', count);
        fmt = fallback;
        goto lbl_normal;

    lbl_done:
    return count;
}