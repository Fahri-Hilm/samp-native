// Minimal SAU API surface used by SampSharp glue.
// Shapes transcribed from SanAndreasUnity source (dev branch):
// Ped.PlayerModel, PedModel.Load(int), PedManager.Instance.pedPrefab,
// ChatManager.singleton + OnChatMessageReceivedOnLocalPlayer, ChatMessage.
// Bodies inert — real behaviour comes from SAU in the Unity player.
using UnityEngine;

namespace SanAndreasUnity.Behaviours
{
    public class PedModel
    {
        public void Load(int id) { }
    }

    public class Ped : MonoBehaviour
    {
        public PedModel PlayerModel { get; private set; } = new PedModel();
    }

    public class PedManager : MonoBehaviour
    {
        public static PedManager Instance { get; private set; }
        public GameObject pedPrefab;
    }
}

namespace SanAndreasUnity.Chat
{
    public class ChatMessage
    {
        public ChatMessage(string msg, string sender)
        {
            this.msg = msg;
            this.sender = sender;
        }
        public string msg;
        public string sender;
    }

    public class ChatManager : MonoBehaviour
    {
        public static ChatManager singleton { get; private set; }
        internal void OnChatMessageReceivedOnLocalPlayer(ChatMessage m) { }
    }
}
