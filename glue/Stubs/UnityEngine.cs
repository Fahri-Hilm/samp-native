// Minimal UnityEngine API surface used by SampSharp glue.
// Signatures mirror Unity; bodies are inert (compile-check only —
// real behaviour comes from the Unity player).
using System;

namespace UnityEngine
{
    public struct Vector3
    {
        public float x, y, z;
        public Vector3(float x, float y, float z)
        { this.x = x; this.y = y; this.z = z; }
        public static Vector3 zero => new Vector3(0, 0, 0);
        public static Vector3 Lerp(Vector3 a, Vector3 b, float t) =>
            new Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                        a.z + (b.z - a.z) * t);
    }

    public struct Quaternion
    {
        public float x, y, z, w;
        public Vector3 eulerAngles =>
            new Vector3(0, 0, 0); // stub: real value from Unity runtime
    }

    public class Transform
    {
        public Vector3 position { get; set; }
        public Quaternion rotation { get; set; }
    }

    public class Object
    {
        public static void Destroy(Object o) { }
        public static T Instantiate<T>(T original) where T : Object =>
            original;
    }

    public class GameObject : Object
    {
        public string name { get; set; }
        public Transform transform { get; } = new Transform();
        public T GetComponent<T>() where T : class => null;
    }

    public class MonoBehaviour : Object
    {
        public GameObject gameObject { get; } = new GameObject();
        public Transform transform => gameObject.transform;
        public T GetComponent<T>() where T : class => null;
        protected static new T Instantiate<T>(T original) where T : Object =>
            original;
        public bool enabled { get; set; }
    }

    public static class Debug
    {
        public static void Log(object o) { }
        public static void LogError(object o) { }
    }

    [AttributeUsage(AttributeTargets.Field)]
    public sealed class HeaderAttribute : Attribute
    {
        public HeaderAttribute(string s) { }
    }

    public static class ObjectExtensions { }
}
