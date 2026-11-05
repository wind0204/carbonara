struct Transform_4x4 {
  row_major float4x4 mat;
};

struct PushConstants {
  [[vk::offset(0)]] struct Transform_4x4 mat1;
};

[[vk::push_constant]] PushConstants consts;

struct VertexShaderInput {
  [[vk::location(0)]] float3 pos : POSITION0;
  //[[vk::location(1)]] float3 col : COLOR0;
  [[vk::location(1)]] float2 tex_coords : TEXCOORD0;
  //[[vk::location(2)]] uint tex_id : TEXCOORD1;
};

//struct UBO {
//  float4x4 model_to_clip;
//};
//cbuffer ubo : register(b0, space0) { UBO ubo; }

struct VertexShaderOutput {
  float4 pos : SV_POSITION;
  //[[vk::location(0)]] float3 col : COLOR0;
  [[vk::location(0)]] float2 tex_coords : TEXCOORD0;
  [[vk::location(1)]] nointerpolation uint tex_id : TEXCOORD1;
};


//-------------------------------------------------------------------------------------------------

[shader("vertex")]
VertexShaderOutput vert_main(
    VertexShaderInput input,
    uint instance_id : SV_InstanceID,
    uint base_instance_id : SV_StartInstanceLocation
    )
{
  VertexShaderOutput output;
  output.pos = mul( float4(input.pos.xyz, 1.0), consts.mat1.mat );
  //output.pos = float4(input.pos.xyz, 1.0);
  //output.col = input.col;
  output.tex_coords = input.tex_coords;
  output.tex_id = base_instance_id;
  return output;
}

// For combined sampler array:
//Texture2DArray texture_arr : register(t0);
//SamplerState std_sampler : register(s0);

[[vk::binding(0, 0)]]  // binding 0, descriptor set 0
Texture2D textures[] : register(t0, space0);
[[vk::binding(0, 1)]]  // binding 0, descriptor set 1
SamplerState immut_sampler : register(s0, space1);

[shader("pixel")]
float4 frag_main( VertexShaderOutput input ) : SV_TARGET
{
  // You would write these lines with a combind sampler array:
  //float3 sample_coords = float3( input.tex_coords.x, input.tex_coords.y, input.tex_id );
  //float4 color = texture_arr.Sample( std_sampler, sample_coords );

  // A simple texture sampler call
  float4 color = textures[input.tex_id].Sample(immut_sampler, input.tex_coords);
  //float4 color = textures[input.tex_id].SampleLevel(immut_sampler, input.tex_coords, 0);
  // TODO: Try this when sparse image is impelemented
  //uint residency_status;
  //float4 color = textures[input.tex_id].SampleLevel(
  //        immut_sampler, input.tex_coords, 10, 0, residency_status );


  return color;
}
