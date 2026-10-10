// Generated from include/twodog/scripting/Api.def. Do not edit.
#nullable enable
using System.Text.Json.Nodes;
namespace TwoDog;
public static class RawApi
{
    public const int Version = 1;
    // Request: level:string,message:string -> null
    public static JsonNode? Log(JsonObject request) => Native.Call(1, request);
    // Request: name:string -> string
    public static JsonNode? Find(JsonObject request) => Native.Call(2, request);
    // Request: entity:string -> bool
    public static JsonNode? Alive(JsonObject request) => Native.Call(3, request);
    // Request: entity:string -> Transform{position:vec2,size:vec2,rotation:float}
    public static JsonNode? GetTransform(JsonObject request) => Native.Call(4, request);
    // Request: entity:string,transform:Transform -> null
    public static JsonNode? SetTransform(JsonObject request) => Native.Call(5, request);
    // Request: entity:EntityDocument -> string
    public static JsonNode? Create(JsonObject request) => Native.Call(6, request);
    // Request: entity:string -> null
    public static JsonNode? Destroy(JsonObject request) => Native.Call(7, request);
    // Request: prefab:string,position:vec2 -> string
    public static JsonNode? Instantiate(JsonObject request) => Native.Call(8, request);
    // Request: action:string -> Action{down:bool,pressed:bool,released:bool}
    public static JsonNode? Input(JsonObject request) => Native.Call(9, request);
    // Request: axis:int -> float
    public static JsonNode? Axis(JsonObject request) => Native.Call(10, request);
    // Request:  -> vec2
    public static JsonNode? Pointer(JsonObject request) => Native.Call(11, request);
    // Request: entity:string -> vec2
    public static JsonNode? GetVelocity(JsonObject request) => Native.Call(12, request);
    // Request: entity:string,value:vec2 -> null
    public static JsonNode? SetVelocity(JsonObject request) => Native.Call(13, request);
    // Request: entity:string,value:vec2 -> null
    public static JsonNode? Impulse(JsonObject request) => Native.Call(14, request);
    // Request: entity:string,value:vec2 -> null
    public static JsonNode? Force(JsonObject request) => Native.Call(15, request);
    // Request: origin:vec2,translation:vec2,ignore?:string -> Hit{entity:string,point:vec2,normal:vec2,fraction:float}|null
    public static JsonNode? Raycast(JsonObject request) => Native.Call(16, request);
    // Request: asset:string,bus:string,loop:bool -> string
    public static JsonNode? PlayAudio(JsonObject request) => Native.Call(17, request);
    // Request: voice:string -> null
    public static JsonNode? StopAudio(JsonObject request) => Native.Call(18, request);
    // Request: bus:string,value:float -> null
    public static JsonNode? Volume(JsonObject request) => Native.Call(19, request);
    // Request: asset:string -> null
    public static JsonNode? LoadScene(JsonObject request) => Native.Call(20, request);
    // Request: path:string -> null
    public static JsonNode? SaveScene(JsonObject request) => Native.Call(21, request);
    // Request: entity:string,component:string -> object|null
    public static JsonNode? GetComponent(JsonObject request) => Native.Call(22, request);
    // Request: entity:string,component:string,value:object -> null
    public static JsonNode? SetComponent(JsonObject request) => Native.Call(23, request);
    // Request: entity:string,topic:string,payload:JSON -> null
    public static JsonNode? Send(JsonObject request) => Native.Call(24, request);
    // Request: center?:vec2,zoom?:float,follow?:string,shake?:float,bounds?:vec4,smoothing?:float,pixelPerfect?:bool,pixelsPerUnit?:float -> null
    public static JsonNode? Camera(JsonObject request) => Native.Call(25, request);
    // Request: position:vec2,count:int,color:vec4 -> null
    public static JsonNode? Emit(JsonObject request) => Native.Call(26, request);
    // Request: entity:string,state:string -> null
    public static JsonNode? SetState(JsonObject request) => Native.Call(27, request);
    // Request:  -> string[]
    public static JsonNode? ListEntities(JsonObject request) => Native.Call(28, request);
    // Request: entity:string,collider:ColliderDocument -> null
    public static JsonNode? AddCollider(JsonObject request) => Native.Call(29, request);
    // Request: joint:JointDocument -> string
    public static JsonNode? CreateJoint(JsonObject request) => Native.Call(30, request);
    // Request: joint:string -> null
    public static JsonNode? DestroyJoint(JsonObject request) => Native.Call(31, request);
    // Request: queries:Request[] -> JSON[]
    public static JsonNode? BatchRead(JsonObject request) => Native.Call(32, request);
    // Request: action:string,bindings:Binding[] -> null
    public static JsonNode? BindAction(JsonObject request) => Native.Call(33, request);
}
