#include <iostream>

#include "../../include/vk.h"

namespace {

auto text_length(const char* text) -> std::size_t
{
    if (text == nullptr) {
        return 0;
    }

    std::size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void write_raw(const char* data, std::size_t count)
{
    if (data == nullptr || count == 0) {
        return;
    }

    const vk_api_t* api = vk_get_api();
    if (api == nullptr) {
        return;
    }

    if (api->vk_stdio_write != nullptr) {
        (void)api->vk_stdio_write(data, static_cast<vk_usize>(count));
        return;
    }

    if (api->vk_putc != nullptr) {
        for (std::size_t index = 0; index < count; ++index) {
            api->vk_putc(data[index]);
        }
    }
}

auto write_unsigned(std::ostream& stream, unsigned long long value) -> std::ostream&
{
    char digits[32];
    std::size_t length = 0;

    if (value == 0) {
        digits[length++] = '0';
    } else {
        while (value > 0) {
            digits[length++] = static_cast<char>('0' + (value % 10ULL));
            value /= 10ULL;
        }
    }

    char buffer[32];
    for (std::size_t index = 0; index < length; ++index) {
        buffer[index] = digits[length - index - 1];
    }
    return stream.write(buffer, length);
}

auto write_signed(std::ostream& stream, long long value) -> std::ostream&
{
    if (value < 0) {
        stream.put('-');
        const unsigned long long magnitude =
            static_cast<unsigned long long>(-(value + 1LL)) + 1ULL;
        return write_unsigned(stream, magnitude);
    }

    return write_unsigned(stream, static_cast<unsigned long long>(value));
}

auto write_pointer(std::ostream& stream, const void* value) -> std::ostream&
{
    static constexpr char kHexDigits[] = "0123456789abcdef";

    const unsigned long long raw =
        static_cast<unsigned long long>(reinterpret_cast<unsigned long>(value));

    char digits[2 + (sizeof(unsigned long long) * 2)];
    std::size_t length = 0;

    digits[length++] = '0';
    digits[length++] = 'x';

    bool seen_nonzero = false;
    for (int shift = static_cast<int>(sizeof(unsigned long long) * 8) - 4;
         shift >= 0;
         shift -= 4) {
        const unsigned digit = static_cast<unsigned>((raw >> shift) & 0xFULL);
        if (digit != 0 || seen_nonzero || shift == 0) {
            seen_nonzero = true;
            digits[length++] = kHexDigits[digit];
        }
    }

    return stream.write(digits, length);
}

}  // namespace

namespace std {

ostream cout;
ostream cerr;

auto ostream::put(char ch) -> ostream&
{
    write_raw(&ch, 1);
    return *this;
}

auto ostream::write(const char* data, size_t count) -> ostream&
{
    write_raw(data, count);
    return *this;
}

void ostream::flush()
{
}

auto operator<<(ostream& stream, ostream_manipulator manip) -> ostream&
{
    return manip(stream);
}

auto operator<<(ostream& stream, const char* text) -> ostream&
{
    if (text == nullptr) {
        return stream.write("(null)", 6);
    }
    return stream.write(text, text_length(text));
}

auto operator<<(ostream& stream, const string& text) -> ostream&
{
    return stream.write(text.data(), text.size());
}

auto operator<<(ostream& stream, char ch) -> ostream&
{
    return stream.put(ch);
}

auto operator<<(ostream& stream, signed char ch) -> ostream&
{
    return stream.put(static_cast<char>(ch));
}

auto operator<<(ostream& stream, unsigned char ch) -> ostream&
{
    return stream.put(static_cast<char>(ch));
}

auto operator<<(ostream& stream, bool value) -> ostream&
{
    return stream << (value ? "true" : "false");
}

auto operator<<(ostream& stream, short value) -> ostream&
{
    return write_signed(stream, static_cast<long long>(value));
}

auto operator<<(ostream& stream, unsigned short value) -> ostream&
{
    return write_unsigned(stream, static_cast<unsigned long long>(value));
}

auto operator<<(ostream& stream, int value) -> ostream&
{
    return write_signed(stream, static_cast<long long>(value));
}

auto operator<<(ostream& stream, unsigned int value) -> ostream&
{
    return write_unsigned(stream, static_cast<unsigned long long>(value));
}

auto operator<<(ostream& stream, long value) -> ostream&
{
    return write_signed(stream, static_cast<long long>(value));
}

auto operator<<(ostream& stream, unsigned long value) -> ostream&
{
    return write_unsigned(stream, static_cast<unsigned long long>(value));
}

auto operator<<(ostream& stream, long long value) -> ostream&
{
    return write_signed(stream, value);
}

auto operator<<(ostream& stream, unsigned long long value) -> ostream&
{
    return write_unsigned(stream, value);
}

auto operator<<(ostream& stream, const void* value) -> ostream&
{
    return write_pointer(stream, value);
}

auto endl(ostream& stream) -> ostream&
{
    stream.put('\n');
    stream.flush();
    return stream;
}

auto flush(ostream& stream) -> ostream&
{
    stream.flush();
    return stream;
}

}  // namespace std
