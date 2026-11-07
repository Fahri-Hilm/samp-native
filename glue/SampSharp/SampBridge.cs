// SAMP backend for San Andreas Unity — replaces the Mirror connection
// with our native SAMP client (libsamp_native.so).
//
// Wiring (see GLUE.md):
//   server text  -> ChatManager.singleton.OnChatMessageReceivedOnLocalPlayer
//   local ped    -> instantiated from PedManager.Instance.pedPrefab,
//                   skinned via PlayerModel.Load, driven by SAU character
//                   controller; transform pushed via samp_set_local_pos
//   remote peds  -> one visual Ped per SAMP player id, transform set from
//                   sync snapshots (lerp hook point in RemotePed.cs)
//   chat out     -> SampBridge.SendChat (wire to ChatInputController in Unity)
//
// Runs Mirror-free: no NetworkBehaviour, no server needed. Put this
// component on a scene GameObject next to SAU's Startup loader and set the
// GTA path + server in the inspector (host/port/nick below).
using System;
using System.Collections.Generic;
using UnityEngine;
using SampNative;
using SanAndreasUnity.Behaviours;
using SanAndreasUnity.Chat;

namespace SampGlue
{
    public class SampBridge : MonoBehaviour
    {
        [Header("SAMP server (or SAMP_HOST / SAMP_PORT / SAMP_NICK env)")]
        public string host = "127.0.0.1";
        public int port = 7777;
        public string nick = "NativeSAU";

        // Self-bootstrap: no scene edit needed. Runs on player start,
        // creates the bridge object if the scene doesn't have one.
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        private static void AutoStart()
        {
            var go = new GameObject("SAMP_Bridge");
            UnityEngine.Object.DontDestroyOnLoad(go);
            go.AddComponent<SampBridge>();
        }

        [Header("Local player")]
        public GameObject localPedRoot;
        public int fallbackSkinId = 0;

        private IntPtr _client = IntPtr.Zero;
        private bool _classRequested;
        private bool _spawnRequested;
        private GameObject _localPed;
        private readonly Dictionary<ushort, GameObject> _remotePeds =
            new Dictionary<ushort, GameObject>();

        private readonly Api.ChatCb _chatCb;
        private readonly Api.RelayCb _relayCb;
        public SampBridge()
        {
            _chatCb = OnServerChat;
            _relayCb = OnPlayerChat;
        }

        void OnEnable()
        {
            string e;
            if (!string.IsNullOrEmpty(e = System.Environment.GetEnvironmentVariable("SAMP_HOST")))
                host = e;
            if (!string.IsNullOrEmpty(e = System.Environment.GetEnvironmentVariable("SAMP_PORT")) &&
                int.TryParse(e, out int p))
                port = p;
            if (!string.IsNullOrEmpty(e = System.Environment.GetEnvironmentVariable("SAMP_NICK")))
                nick = e;
            _client = Api.samp_create();
            Api.samp_set_chat_cb(_client, _chatCb, IntPtr.Zero);
            Api.samp_set_relay_cb(_client, _relayCb, IntPtr.Zero);
            if (Api.samp_connect(_client, SampMarshal.ToUtf8(host),
                                 (ushort)port, SampMarshal.ToUtf8(nick)) != 0)
                Debug.LogError("[samp] connect failed");
        }

        void OnDisable()
        {
            if (_client != IntPtr.Zero) {
                Api.samp_destroy(_client);
                _client = IntPtr.Zero;
            }
            foreach (var kv in _remotePeds)
                if (kv.Value != null) UnityEngine.Object.Destroy(kv.Value);
            _remotePeds.Clear();
            if (_localPed != null) UnityEngine.Object.Destroy(_localPed);
            _localPed = null;
        }

        private int _frame;
    private bool _announced;
        void Update()
        {
            if (_client == IntPtr.Zero) return;
            Api.samp_poll(_client);
            if (Api.samp_failed(_client) != 0) {
                Debug.LogError("[samp] link failed");
                enabled = false;
                return;
            }
            int st = Api.samp_state(_client);
            if (st == 3 && !_classRequested) {
                Api.samp_request_class(_client, 0);
                _classRequested = true;
            }
            if (st >= 4 && Api.samp_spawn_received(_client) != 0 &&
                !_spawnRequested) {
                Api.samp_request_spawn(_client);
                _spawnRequested = true;
                SpawnLocalPed();
            }
            if (st == 5) {
                if (!_announced) {
                    _announced = true;
                    Api.samp_send_chat(_client,
                        SampMarshal.ToUtf8("SAU online (Unity native)"));
                    Debug.Log("[samp] announced presence");
                }
                PushLocalTransform();
                int sent = Api.samp_push_state(_client);
                if ((_frame++ % 300) == 0)
                    Debug.Log("[samp] tick st=" + st + " pushed=" + sent +
                              " remotes=" + Api.samp_remote_count(_client));
                SyncRemotePeds();
            }
        }

        // Called by UI instead of ChatSync.SendChatMsgToServer.
        public void SendChat(string msg)
        {
            if (_client != IntPtr.Zero)
                Api.samp_send_chat(_client, SampMarshal.ToUtf8(msg));
        }

        private void OnServerChat(IntPtr utf8, uint color, IntPtr ctx)
        {
            var mgr = ChatManager.singleton;
            if (mgr != null)
                mgr.OnChatMessageReceivedOnLocalPlayer(
                    new ChatMessage(SampMarshal.FromUtf8(utf8),
                                    string.Empty));
        }

        private void OnPlayerChat(ushort senderId, IntPtr utf8, IntPtr ctx)
        {
            var mgr = ChatManager.singleton;
            if (mgr != null)
                mgr.OnChatMessageReceivedOnLocalPlayer(
                    new ChatMessage(SampMarshal.FromUtf8(utf8),
                                    "Player " + senderId));
        }

        private void SpawnLocalPed()
        {
            if (_localPed != null) return;
            GameObject prefab = null;
            if (PedManager.Instance != null)
                prefab = PedManager.Instance.pedPrefab;
            if (prefab == null) {
                Debug.LogError("[samp] no ped prefab (PedManager missing?)");
                return;
            }
            _localPed = localPedRoot != null ? localPedRoot
                : Instantiate(prefab);
            var ped = _localPed.GetComponent<Ped>();
            if (ped != null) ped.PlayerModel.Load(fallbackSkinId);
        }

        private void PushLocalTransform()
        {
            if (_localPed == null) return;
            Vector3 p = _localPed.transform.position;
            float yaw = _localPed.transform.rotation.eulerAngles.y;
            Api.samp_set_local_pos(_client, p.x, p.y, p.z, yaw);
        }

        private readonly List<ushort> _scratch = new List<ushort>();

        private void SyncRemotePeds()
        {
            _scratch.Clear();
            int n = Api.samp_remote_count(_client);
            var seen = new HashSet<ushort>();
            for (int i = 0; i < n; ++i) {
                ushort id;
                float x, y, z, hp;
                if (Api.samp_remote_get(_client, i, out id, out x, out y,
                                        out z, out hp) != 0)
                    continue;
                seen.Add(id);
                GameObject go;
                if (!_remotePeds.TryGetValue(id, out go) || go == null)
                    go = SpawnRemotePed(id);
                if (go == null) continue;
                _remotePeds[id] = go;
                go.transform.position = Vector3.Lerp(go.transform.position,
                    new Vector3(x, y, z), 0.35f);
            }
            foreach (var kv in _remotePeds)
                if (!seen.Contains(kv.Key)) _scratch.Add(kv.Key);
            foreach (ushort id in _scratch) {
                if (_remotePeds[id] != null) UnityEngine.Object.Destroy(_remotePeds[id]);
                _remotePeds.Remove(id);
            }
        }

        private GameObject SpawnRemotePed(ushort id)
        {
            if (PedManager.Instance == null ||
                PedManager.Instance.pedPrefab == null) {
                Debug.LogError("[samp] no ped prefab for remote ped");
                return null;
            }
            var go = Instantiate(PedManager.Instance.pedPrefab);
            go.name = "SAMP_Remote_" + id;
            var ped = go.GetComponent<Ped>();
            if (ped != null) ped.PlayerModel.Load(fallbackSkinId);
            return go;
        }
    }
}
