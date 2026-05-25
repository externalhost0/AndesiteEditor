struct VSInput {
    uint VertexID : SV_VertexID;
};
struct FSOutput {
    float4 FragColor : SV_Target0;
};
struct v2f {
    // shader predefined
    float4 ClipPos : SV_Position;
    // defined by me
    float3 Normal           : NORMAL;
    float2 UV               : TEXCOORD0;
    float3 WorldPosition    : POSITION;
};
struct Vertex {
    float3 position;
    float uv_x;
    float3 normal;
    float uv_y;
    float4 tangent;
};

struct Camera {
    float4x4 proj;
    float4x4 view;
    float3 position;
}
struct PerFrameData {
    Camera camera;
    float time;
    float2 resolution;
}

struct UserData {
    float4 color;
}
struct PushConstants {
    // provided data
    float4x4 model;
    Ptr<Vertex> vertexBufferAddress;
    Ptr<PerFrameData> frame;
    DescriptorHandle<SamplerState> linearSampler;
    DescriptorHandle<SamplerState> nearestSampler;
    Ptr<UserData> user;
}

[[vk::push_constant]]
PushConstants push;

[shader("vertex")]
v2f vs_main(VSInput input) {
    v2f output;

    Vertex v = push.vertexBufferAddress[input.VertexID];

    float3 worldpos = mul(push.model, float4(v.position, 1)).xyz;
    output.ClipPos = mul(mul(push.frame.camera.proj, push.frame.camera.view), float4(worldpos, 1));

    output.Normal = mul(push.model, float4(v.normal, 0)).xyz;
    output.UV = float2(v.uv_x, v.uv_y);
    output.WorldPosition = worldpos;
    return output;
}