struct VertexShaderInput {
  [[vk::location(0)]] float3 pos : POSITION0;
  [[vk::location(1)]] float3 col : COLOR0;
};

//struct UBO {
//  float4x4 model_to_clip;
//};
//cbuffer ubo : register(b0, space0) { UBO ubo; }

struct VertexShaderOutput {
  float4 pos : SV_POSITION;
  [[vk::location(0)]] float3 col : COLOR0;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput vert_main(
    VertexShaderInput input,
    uint vert_idx : SV_VertexID )
{
  VertexShaderOutput output;
  //output.pos = mul( ubo.model_to_clip, float4(input.position.xyz, 1.0) );
  output.pos = float4(input.pos.xyz, 1.0);
  output.col = input.col;
  return output;
}

[shader("pixel")]
float4 frag_main( VertexShaderOutput input ) : SV_TARGET
{
  //output.pos = mul( ubo.model_to_clip, float4(input.position.xyz, 1.0) );
  return float4(input.col, 1.0);
}
