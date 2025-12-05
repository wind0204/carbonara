struct VSInput {
  [[vk::location(0)]] float3 position : POSITION0;
  //[[vk::location(1)]] float3 color : COLOR0;
};

//struct UBO {
//  float4x4 model_to_clip;
//};
//cbuffer ubo : register(b0, space0) { UBO ubo; }

struct VSOutput {
  float4 pos : SV_POSITION;
  //[[vk::location(0)]] float3 col : COLOR0;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VSOutput vertex_main(
    VSInput input,
    uint vert_idx : SV_VertexID )
{
  VSOutput output;
  //output.pos = mul( ubo.model_to_clip, float4(input.position.xyz, 1.0) );
  output.pos = float4(input.position.xyz, 1.0);
  return output;
}
