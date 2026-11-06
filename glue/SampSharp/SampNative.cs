// P/Invoke surface for libsamp_native.so (see port/net/samp_native_api.h).
// Signatures must match the C ABI byte-for-byte.
//
// Strings: C side is NUL-terminated UTF-8. netstandard2.0 has no
// LPUTF8Str, so sends go as byte[] (marshalled as char*) and receives
// arrive as IntPtr decoded manually (SampMarshal below). Unity's Mono
// honours the same layout, so this assembly works unchanged in-editor.
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace SampNative
{
    internal static class SampMarshal
    {
        internal static byte[] ToUtf8(string s)
        {
            if (s == null) return new byte[1];
            byte[] body = Encoding.UTF8.GetBytes(s);
            var nul = new byte[body.Length + 1];
            Buffer.BlockCopy(body, 0, nul, 0, body.Length);
            return nul;
        }

        internal static string FromUtf8(IntPtr p)
        {
            if (p == IntPtr.Zero) return string.Empty;
            int n = 0;
            while (Marshal.ReadByte(p, n) != 0) ++n;
            var buf = new byte[n];
            Marshal.Copy(p, buf, 0, n);
            return Encoding.UTF8.GetString(buf);
        }
    }

    internal static class Api
    {
        private const string Lib = "samp_native";

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr samp_create();

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void samp_destroy(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_connect(IntPtr c,
            [MarshalAs(UnmanagedType.LPArray)] byte[] host,
            ushort port,
            [MarshalAs(UnmanagedType.LPArray)] byte[] nick);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_request_class(IntPtr c, int cls);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_request_spawn(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void samp_poll(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_failed(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_state(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_spawn_received(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_send_chat(IntPtr c,
            [MarshalAs(UnmanagedType.LPArray)] byte[] utf8);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void samp_set_local_pos(IntPtr c,
            float x, float y, float z, float rot);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_push_state(IntPtr c);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void ChatCb(IntPtr utf8, uint color, IntPtr ctx);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void RelayCb(ushort senderId, IntPtr utf8,
            IntPtr ctx);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void samp_set_chat_cb(IntPtr c,
            ChatCb cb, IntPtr ctx);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void samp_set_relay_cb(IntPtr c,
            RelayCb cb, IntPtr ctx);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_remote_count(IntPtr c);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_remote_get(IntPtr c, int index,
            out ushort id, out float x, out float y, out float z,
            out float hp);

        [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int samp_roster_count(IntPtr c);
    }
}
