struct VertexShaderInput_cp2sc {
  [[vk::location(0)]] float3 pos : POSITION0;
  [[vk::location(1)]] float2 tex_uv : TEXCOORD0;
};

struct VertexShaderOutput_cp2sc {
  noperspective float4 pos : SV_POSITION;
  noperspective [[vk::location(0)]] float2 tex_uv : TEXCOORD0;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput_cp2sc vert_main(
    VertexShaderInput_cp2sc input,
    uint vert_idx : SV_VertexID )
{
  VertexShaderOutput_cp2sc output;
  output.pos = float4(input.pos.xyz, 1.0);
  output.tex_uv = input.tex_uv;
  return output;
}

//-------------------------------------------------------------------------------------------------

Texture2D render_tgt : register(t0);
SamplerState tex_sampler : register(s0);
[shader("pixel")]
float4 frag_main( VertexShaderOutput_cp2sc input ) : SV_TARGET
{
  float4 output = render_tgt.Sample(tex_sampler, input.tex_uv);
  return output;
}
