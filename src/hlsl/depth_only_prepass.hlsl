struct VertexShaderInput_do {
  [[vk::location(0)]] float3 pos : POSITION0;
};

struct VertexShaderOutput_do {
  float4 pos : SV_POSITION;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput_do vert_main(
    VertexShaderInput_do input,
    uint vert_idx : SV_VertexID )
{
  VertexShaderOutput_do output;
  output.pos = float4(input.pos.xyz, 1.0);
  return output;
}

[shader("pixel")]
void frag_main()
{
  discard;
}
