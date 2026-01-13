//struct VertexShaderInput {
//  [[vk::location(0)]] float3 pos : POSITION0;
//  [[vk::location(1)]] float2 tex_uv : TEXCOORD0;
//};

struct VertexShaderOutput {
  noperspective float4 pos : SV_POSITION;
  [[vk::location(0)]] float2 tex_uv : TEXCOORD0;
};
//struct VertexShaderOutput {
//  noperspective float4 pos : SV_POSITION;
//  noperspective [[vk::location(0)]] float2 tex_uv : TEXCOORD0;
//};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput vert_main( uint vert_idx : SV_VertexID )
{
  VertexShaderOutput output;
  output.tex_uv = float2((vert_idx << 1) & 2, vert_idx & 2);
  output.pos = float4(output.tex_uv * 2.0f + -1.0f, 0.0f, 1.0f);

  return output;
}
//[shader("vertex")]
//VertexShaderOutput vert_main(
//    VertexShaderInput input,
//    uint vert_idx : SV_VertexID )
//{
//  VertexShaderOutput output;
//  output.pos = float4(input.pos.xyz, 1.0);
//  output.tex_uv = input.tex_uv;
//
//  return output;
//}

//-------------------------------------------------------------------------------------------------

Texture2D render_tgt : register(t0);
SamplerState tex_sampler : register(s0);
[shader("pixel")]
float4 frag_main( VertexShaderOutput input ) : SV_TARGET
{
  float4 output = render_tgt.Sample(tex_sampler, input.tex_uv);
  return output;
}
