#pragma once
// ─────────────────────────────────────────────────────────────────────────
//  Copy this file to secrets.h and fill in real values.
//  secrets.h is gitignored — never commit it.
//
//  IMPORTANT: Every CIPHER node in the same mesh must use the SAME crypto
//  key values (AP_PASSWORD can differ per node; keys must match).
// ─────────────────────────────────────────────────────────────────────────

// WiFi AP password — "" = open (anyone nearby can connect without a password)
#define AP_PASSWORD     ""

// ─── Shared mesh encryption keys ─────────────────────────────────────────
// All nodes must match. Generate with a CSPRNG before field deploy.
// Example generation (Linux/Mac): openssl rand -base64 32
#define PROTO_REQUIRE_AUTH        1
#define PROTO_REQUIRE_ENCRYPTION  1
#define PROTO_AES_KEY_TEXT        "REPLACE_WITH_BASE64_32BYTE_KEY_FROM_CSPRNG="
#define PROTO_AUTH_KEY_A          0xDEADBEEFUL
#define PROTO_AUTH_KEY_B          0xCAFEBABEUL
#define PROTO_AUTH_KEY_C          0xFEEDFACEUL
#define PROTO_AUTH_KEY_D          0xC0FFEE00UL
