#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace outsider {

enum class WebSocketOpcode : std::uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
};

enum class WebSocketParseResult : std::uint8_t {
    Incomplete = 0,
    Ok,
    ProtocolError
};

struct WebSocketFrame {
    WebSocketOpcode opcode = WebSocketOpcode::Text;
    std::string payload;
    bool fin = true;
    bool masked = false;
};

inline std::uint32_t websocket_xorshift32(std::uint32_t value) {
    if (value == 0) {
        value = 0x6d2b79f5u;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

inline std::string websocket_base64_encode(std::string_view input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    for (std::size_t i = 0; i < input.size(); i += 3) {
        const std::uint32_t b0 = static_cast<unsigned char>(input[i]);
        const std::uint32_t b1 = (i + 1 < input.size()) ? static_cast<unsigned char>(input[i + 1]) : 0u;
        const std::uint32_t b2 = (i + 2 < input.size()) ? static_cast<unsigned char>(input[i + 2]) : 0u;
        const std::uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out.push_back(kAlphabet[(triple >> 18) & 0x3fu]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3fu]);
        out.push_back(i + 1 < input.size() ? kAlphabet[(triple >> 6) & 0x3fu] : '=');
        out.push_back(i + 2 < input.size() ? kAlphabet[triple & 0x3fu] : '=');
    }

    return out;
}

inline std::array<std::uint8_t, 20> websocket_sha1(std::string_view input) {
    std::uint64_t bit_count = static_cast<std::uint64_t>(input.size()) * 8u;
    std::string padded(input);
    padded.push_back(static_cast<char>(0x80));
    while ((padded.size() % 64u) != 56u) {
        padded.push_back('\0');
    }
    for (int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<char>((bit_count >> (i * 8)) & 0xffu));
    }

    std::uint32_t h0 = 0x67452301u;
    std::uint32_t h1 = 0xefcdab89u;
    std::uint32_t h2 = 0x98badcfeu;
    std::uint32_t h3 = 0x10325476u;
    std::uint32_t h4 = 0xc3d2e1f0u;

    for (std::size_t chunk = 0; chunk < padded.size(); chunk += 64) {
        std::uint32_t w[80]{};
        for (int i = 0; i < 16; ++i) {
            const std::size_t base = chunk + static_cast<std::size_t>(i) * 4u;
            w[i] = (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[base])) << 24) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[base + 1])) << 16) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[base + 2])) << 8) |
                   static_cast<std::uint32_t>(static_cast<unsigned char>(padded[base + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            const std::uint32_t value = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (value << 1) | (value >> 31);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdcu;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6u;
            }

            const std::uint32_t rot_a = (a << 5) | (a >> 27);
            const std::uint32_t rot_b = (b << 30) | (b >> 2);
            const std::uint32_t temp = rot_a + f + e + k + w[i];
            e = d;
            d = c;
            c = rot_b;
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::array<std::uint8_t, 20> digest{};
    const std::uint32_t words[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        digest[static_cast<std::size_t>(i) * 4u] = static_cast<std::uint8_t>((words[i] >> 24) & 0xffu);
        digest[static_cast<std::size_t>(i) * 4u + 1u] = static_cast<std::uint8_t>((words[i] >> 16) & 0xffu);
        digest[static_cast<std::size_t>(i) * 4u + 2u] = static_cast<std::uint8_t>((words[i] >> 8) & 0xffu);
        digest[static_cast<std::size_t>(i) * 4u + 3u] = static_cast<std::uint8_t>(words[i] & 0xffu);
    }
    return digest;
}

inline std::string websocket_sha1_base64(std::string_view input) {
    const std::array<std::uint8_t, 20> digest = websocket_sha1(input);
    return websocket_base64_encode(
        std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size()));
}

inline std::string websocket_accept_value(std::string_view client_key) {
    static constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined(client_key);
    combined += kGuid;
    return websocket_sha1_base64(combined);
}

inline std::string websocket_make_client_key(std::uint32_t seed) {
    char raw[16];
    std::uint32_t value = seed;
    for (int i = 0; i < 16; ++i) {
        value = websocket_xorshift32(value + static_cast<std::uint32_t>(i) * 0x9e3779b9u);
        raw[i] = static_cast<char>(value & 0xffu);
    }
    return websocket_base64_encode(std::string_view(raw, sizeof(raw)));
}

inline std::string websocket_build_client_request(std::string_view host,
                                                  std::uint16_t port,
                                                  std::string_view path,
                                                  std::string_view client_key) {
    std::string request;
    request.reserve(256);
    request += "GET ";
    request += path.empty() ? "/" : std::string(path);
    request += " HTTP/1.1\r\nHost: ";
    request += host;
    request += ":";
    request += std::to_string(port);
    request += "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ";
    request += client_key;
    request += "\r\n\r\n";
    return request;
}

inline std::string websocket_build_server_response(std::string_view accept_value) {
    std::string response;
    response.reserve(192);
    response += "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
    response += accept_value;
    response += "\r\n\r\n";
    return response;
}

inline std::string websocket_header_value(std::string_view headers, std::string_view name) {
    const std::string needle = std::string(name) + ":";
    std::size_t pos = 0;
    while (pos < headers.size()) {
        const std::size_t line_end = headers.find("\r\n", pos);
        const std::size_t end = line_end == std::string_view::npos ? headers.size() : line_end;
        const std::string_view line = headers.substr(pos, end - pos);
        if (line.size() >= needle.size()) {
            bool match = true;
            for (std::size_t i = 0; i < needle.size(); ++i) {
                char a = line[i];
                char b = needle[i];
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b) {
                    match = false;
                    break;
                }
            }
            if (match) {
                std::size_t value_start = needle.size();
                while (value_start < line.size() && (line[value_start] == ' ' || line[value_start] == '\t')) {
                    ++value_start;
                }
                return std::string(line.substr(value_start));
            }
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        pos = line_end + 2;
    }
    return {};
}

inline bool websocket_validate_server_response(std::string_view response,
                                               std::string_view expected_accept) {
    const std::size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        return false;
    }
    const std::string_view headers = response.substr(0, header_end);
    if (headers.find("101") == std::string_view::npos) {
        return false;
    }
    const std::string accept = websocket_header_value(headers, "Sec-WebSocket-Accept");
    return accept == expected_accept;
}

inline std::string websocket_encode_frame(WebSocketOpcode opcode,
                                          std::string_view payload,
                                          bool mask,
                                          std::uint32_t mask_key = 0) {
    std::string frame;
    frame.reserve(payload.size() + 16);
    frame.push_back(static_cast<char>(0x80u | static_cast<std::uint8_t>(opcode)));

    const std::uint64_t length = static_cast<std::uint64_t>(payload.size());
    const std::uint8_t mask_bit = mask ? 0x80u : 0x00u;
    if (length < 126u) {
        frame.push_back(static_cast<char>(mask_bit | static_cast<std::uint8_t>(length)));
    } else if (length <= 0xffffu) {
        frame.push_back(static_cast<char>(mask_bit | 126u));
        frame.push_back(static_cast<char>((length >> 8) & 0xffu));
        frame.push_back(static_cast<char>(length & 0xffu));
    } else {
        frame.push_back(static_cast<char>(mask_bit | 127u));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<char>((length >> shift) & 0xffu));
        }
    }

    std::string body(payload);
    if (mask) {
        const char mask_bytes[4] = {
            static_cast<char>((mask_key >> 24) & 0xffu),
            static_cast<char>((mask_key >> 16) & 0xffu),
            static_cast<char>((mask_key >> 8) & 0xffu),
            static_cast<char>(mask_key & 0xffu)};
        frame.append(mask_bytes, 4);
        for (std::size_t i = 0; i < body.size(); ++i) {
            body[i] = static_cast<char>(body[i] ^ mask_bytes[i % 4u]);
        }
    }

    frame += body;
    return frame;
}

inline WebSocketParseResult websocket_decode_frame(std::string_view buffer,
                                                   bool require_masked,
                                                   std::size_t* consumed,
                                                   WebSocketFrame* out) {
    if (!consumed || !out) {
        return WebSocketParseResult::ProtocolError;
    }
    *consumed = 0;

    if (buffer.size() < 2) {
        return WebSocketParseResult::Incomplete;
    }

    const std::uint8_t b0 = static_cast<std::uint8_t>(buffer[0]);
    const std::uint8_t b1 = static_cast<std::uint8_t>(buffer[1]);
    const bool fin = (b0 & 0x80u) != 0;
    const WebSocketOpcode opcode = static_cast<WebSocketOpcode>(b0 & 0x0fu);
    const bool masked = (b1 & 0x80u) != 0;
    std::uint64_t length = static_cast<std::uint64_t>(b1 & 0x7fu);
    std::size_t offset = 2;

    if (!fin) {
        return WebSocketParseResult::ProtocolError;
    }
    if (require_masked && !masked) {
        return WebSocketParseResult::ProtocolError;
    }

    if (length == 126u) {
        if (buffer.size() < offset + 2u) {
            return WebSocketParseResult::Incomplete;
        }
        length = (static_cast<std::uint64_t>(static_cast<std::uint8_t>(buffer[offset])) << 8) |
                 static_cast<std::uint64_t>(static_cast<std::uint8_t>(buffer[offset + 1u]));
        offset += 2u;
    } else if (length == 127u) {
        if (buffer.size() < offset + 8u) {
            return WebSocketParseResult::Incomplete;
        }
        length = 0;
        for (int i = 0; i < 8; ++i) {
            length = (length << 8) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(buffer[offset + static_cast<std::size_t>(i)]));
        }
        offset += 8u;
    }

    char mask_bytes[4]{};
    if (masked) {
        if (buffer.size() < offset + 4u) {
            return WebSocketParseResult::Incomplete;
        }
        for (int i = 0; i < 4; ++i) {
            mask_bytes[i] = buffer[offset + static_cast<std::size_t>(i)];
        }
        offset += 4u;
    }

    if (buffer.size() < offset + length) {
        return WebSocketParseResult::Incomplete;
    }

    out->opcode = opcode;
    out->fin = fin;
    out->masked = masked;
    out->payload.assign(buffer.substr(offset, static_cast<std::size_t>(length)));
    if (masked) {
        for (std::size_t i = 0; i < out->payload.size(); ++i) {
            out->payload[i] = static_cast<char>(out->payload[i] ^ mask_bytes[i % 4u]);
        }
    }

    *consumed = offset + static_cast<std::size_t>(length);
    return WebSocketParseResult::Ok;
}

}  // namespace outsider
