struct Transform_4x4 {
  row_major float4x4 mat;
};

struct PushConstants {
  struct Transform_4x4 mat1;
};

[[vk::push_constant]] PushConstants consts;

struct VertexShaderInput {
  [[vk::location(0)]] float3 pos : POSITION0;
};

struct VertexShaderOutput {
  float4 pos : SV_POSITION;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput vert_main(
    VertexShaderInput input,
    uint vert_idx : SV_VertexID )
{
  VertexShaderOutput output;
  //output.pos = mul( consts.mat1.mat, float4(input.pos.xyz, 1.0) );
  output.pos = mul( float4(input.pos.xyz, 1.0), consts.mat1.mat );
  //output.pos = float4(input.pos.xyz, 1.0);
  return output;
}

[shader("pixel")]
void frag_main()
{
  return;
}
