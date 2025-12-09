struct VertexShaderInput_cp2sc {
  [[vk::location(0)]] float3 pos : POSITION0;
  //[[vk::location(1)]] float3 col : COLOR0;
};

//struct UBO {
//  float4x4 model_to_clip;
//};
//cbuffer ubo : register(b0, space0) { UBO ubo; }

struct VertexShaderOutput_cp2sc {
  float4 pos : SV_POSITION;
  //[[vk::location(0)]] float3 col : COLOR0;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput_cp2sc vert_main(
    VertexShaderInput_cp2sc input,
    uint vert_idx : SV_VertexID )
{
  VertexShaderOutput_cp2sc output;
  //output.pos = mul( ubo.model_to_clip, float4(input.position.xyz, 1.0) );
  output.pos = float4(input.pos.xyz, 1.0);
  //output.col = input.col;
  return output;
}

[shader("pixel")]
float4 frag_main( VertexShaderOutput_cp2sc input ) : SV_TARGET
{
  return float4(0.1, 0.8, 0.1, 1);
}
