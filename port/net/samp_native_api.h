#pragma once

// C ABI for engine bindings (Unity P/Invoke, Godot GDExtension, ...).
// Single-client model: one global HandshakeClient driven from the game thread.
// All functions are thread-unsafe by design — call only from one thread.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SampClient SampClient;
typedef void (*SampChatCb)(const char* utf8, unsigned color, void* ctx);
typedef void (*SampRelayCb)(unsigned short senderId, const char* utf8,
                            void* ctx);

SampClient* samp_create(void);
void samp_destroy(SampClient* c);

// Connect+join handshake. Returns 0 on start (async), -1 on immediate error.
// Drive with samp_poll() until samp_state() == 5 (spawned).
int samp_connect(SampClient* c, const char* host, unsigned short port,
                 const char* nick);
int samp_request_class(SampClient* c, int cls);
int samp_request_spawn(SampClient* c);
void samp_poll(SampClient* c);
int samp_failed(SampClient* c);

// 0 idle, 1 connecting, 2 accepted, 3 join sent, 4 class requested,
// 5 spawned.
int samp_state(SampClient* c);

// Outgoing.
int samp_send_chat(SampClient* c, const char* utf8);
void samp_set_local_pos(SampClient* c, float x, float y, float z, float rot);

// Incoming chat (server messages). Callback fires inside samp_poll for
// lines not yet delivered. sender is "" for server text.
void samp_set_chat_cb(SampClient* c, SampChatCb cb, void* ctx);
void samp_set_relay_cb(SampClient* c, SampRelayCb cb, void* ctx);

// Remote players snapshot. Returns count (max capped); use index accessors.
int samp_remote_count(SampClient* c);
int samp_remote_get(SampClient* c, int index, unsigned short* id, float* x,
                    float* y, float* z, float* hp);

// Roster (WorldPlayerAdd/Remove).
int samp_roster_count(SampClient* c);
int samp_roster_get(SampClient* c, int index, unsigned short* id,
                    char* nameOut, int nameCap, int* skin);

#ifdef __cplusplus
}
#endif
