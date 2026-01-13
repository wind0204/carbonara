struct Transform_4x4 {
  row_major float4x4 mat;
};

struct PushConstants {
  struct Transform_4x4 mat1;
};

[[vk::push_constant]] PushConstants consts;

struct VertexShaderInput {
  [[vk::location(0)]] float3 pos : POSITION0;
  //[[vk::location(1)]] float3 col : COLOR0;
  [[vk::location(1)]] float2 tex_coords : TEXCOORD0;
  [[vk::location(2)]] uint tex_id : TEXCOORD1;
};

//struct UBO {
//  float4x4 model_to_clip;
//};
//cbuffer ubo : register(b0, space0) { UBO ubo; }

struct VertexShaderOutput {
  float4 pos : SV_POSITION;
  //[[vk::location(0)]] float3 col : COLOR0;
  [[vk::location(0)]] float2 tex_coords : TEXCOORD0;
  [[vk::location(1)]] uint tex_id : TEXCOORD1;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput vert_main(
    VertexShaderInput input,
    uint vert_idx : SV_VertexID )
{
  VertexShaderOutput output;
  output.pos = mul( float4(input.pos.xyz, 1.0), consts.mat1.mat );
  //output.pos = float4(input.pos.xyz, 1.0);
  //output.col = input.col;
  output.tex_coords = input.tex_coords;
  output.tex_id = input.tex_id;
  return output;
}

Texture2DArray texture_arr : register(t0);  // t0 is a texture slot
SamplerState my_sampler : register(s0);     // s0 is a sampler slot
[shader("pixel")]
float4 frag_main( VertexShaderOutput input ) : SV_TARGET
{

  // Combine UV and array index into a float3
  float3 sample_coords = float3( input.tex_coords.x, input.tex_coords.y, input.tex_id ? input.tex_id : 0 );

  // Sample the texture array
  float4 color = texture_arr.Sample( my_sampler, sample_coords );

  return color;
  //return float4(input.col, 1.0);
}
