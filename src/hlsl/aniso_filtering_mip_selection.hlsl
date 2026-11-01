/**
 * \brief https://pema.dev/2025/05/09/mipmaps-too-much-detail/
 */

void ellipsoid_transform_derivatives(inout float2 dx, inout float2 dy)
{
  bool any_zero = length(dx) == 0 || length(dy) == 0;
  bool parallel = (dx.x * dy.y - dx.y * dy.x) == 0;
  bool perpendicular = dot(dx, dy) == 0;
  bool nonFinite = isinf(dx) || isinf(dy) || isnan(dx) || isnan(dy);
  if ( !any_zero && !parallel && !perpendicular && !nonFinite )
  {
    // Proper orthogonal Jacobian matrix should be used
    // Fundamentals of Texture Mapping and Image Warping written by Paul S. Heckbert

    // Implicit ellipse coefficients
    float A = dx.y*dx.y + dy.y*dy.y;
    float B = -2.0 * (dx.x * dx.y + dy.x * dy.y);
    float C = dx.x*dx.x + dy.x*dy.x;
    float F = (dx.x * dy.y - dy.x * dx.y) * (dx.x * dy.y - dy.x * dx.y);

    float p = A - C;
    float q = A + C;
    float t = sqrt(p*p + B*B);

    float2 newDx, newDy;
    newDx.x = sqrt( F * (t+p) / ( t * (q+t)) );
    newDx.y = sqrt( F * (t-p) / ( t * (q+t)) ) * sign(B);
    newDy.x = sqrt( F * (t-p) / ( t * (q-t)) ) * -sign(B);
    newDy.y = sqrt( F * (t+p) / ( t * (q-t)) );

    bool failed = any( isnan(newDx) || isinf(newDx) || isnan(newDy) || isinf(newDy) );
    if ( !failed )
    {
      dx = newDx;
      dy = newDy;
    }
  }
}

float calc_lod_lv(
    float2 tex_size, float2 dx, float2 dy, float aniso_lv,
    out float ratio_of_aniso, out float2 aniso_line_direction )
{
  dx *= tex_size;
  dy *= tex_size;

  // Ellipsoid transform
  ellipsoid_transform_derivatives( dx, dy );

  //// Regular mip selection
  //if ( aniso_lv == 0 )
  //{
  //    float x_len = sqrt( dx.u*dx.u + dx.v*dx.v );
  //    float y_len = sqrt( dy.u*dy.u + dy.v*dy.v );
  //    return log2( max(x_len, y_len) );
  //}
  //// Anisotropic filtering
  //else
  {
    float x_len_squared = dx.u*dx.u + dx.v*dx.v;
    float y_len_squared = dy.u*dy.u + dy.v*dy.v;
    float determinant = abs( dx.u*dy.v - dx.v*dy.u );
    bool is_major_x = x_len_squared > y_len_squared;
    float squared_len_maj = is_major_x ? x_len_squared : y_len_squared;
    float len_major = sqrt( squared_len_maj );

    ratio_of_aniso = squared_len_maj / determinant;
    float len_minor;
    if ( ratio_of_aniso > aniso_lv )
    {
      ratio_of_aniso = aniso_lv;
      len_minor = len_major / ratio_of_aniso;
    }
    else
    {
      len_minor = determinant / len_major;
    }

    float rcp_len_major = 1.f / len_major
    aniso_line_direction.u = (is_major_x ? dx.u : dy.u) * rcp_len_major
    aniso_line_direction.v = (is_major_x ? dx.v : dy.v) * rcp_len_major

    return log2(len_minor);
  }
}


const float2 std_tex_size = float2(2048, 2048);
const float max_aniso_lv = 12;

float4 aniso_sampling_exmaple_fragment_shader()
{
  // Get mip level, aniso line direction, and ratio of anisotropy
  float orig_ratio_of_aniso;
  float2 aniso_line_direction;
  float orig_mip_lv = calc_lod_lv(
      std_tex_size, ddx(input.tex_coords), ddy(input.tex_coords),
      max_aniso_lv, orig_ratio_of_aniso, aniso_line_direction);
  uint residency_status;

  // Take multiple samples and average them
  if ( orig_ratio_of_aniso > 1.0 ) {
    float aniso_idx = 0;
    float mip_lv = orig_mip_lv;
    float ratio_of_aniso = orig_ratio_of_aniso;
    float ratio_of_aniso_rounded;
    float4 sum;
    do {  // while ( !CheckAccessFullyMapped(residency_status) )
      ratio_of_aniso_rounded = round( ratio_of_aniso );

      // Convert ratio to [0; 1] space
      float2 ratio_in_uv_space =
        ratio_of_aniso_rounded * float2(1.0/std_tex_size.u, 1.0/std_tex_size.v);

      // Get start and end positions of the anisotropic line
      float2 start_pos = input.tex_coords - aniso_line_direction * (ratio_in_uv_space / 2.0);
      float2 end_pos = start_pos + aniso_line_direction * ratio_in_uv_space;

      // Get the current sample position
      float2 sample_pos = lerp( start_pos, end_pos, aniso_idx / (ratio_of_aniso_rounded-1) );
      float4 color = textures[input.tex_id].SampleLevel(
          immut_sampler, sample_pos, mip_lv, 0, residency_status );

      // Accumulate the sample
      sum = color;
      mip_lv += 1;
      ratio_of_aniso *= 0.5;
    } while ( !CheckAccessFullyMapped(residency_status) );
    mip_lv -= 1;
    ratio_of_aniso *= 2.0;

    for ( aniso_idx = 1; aniso_idx < ratio_of_aniso_rounded; aniso_idx++ ) {
      // Convert ratio to [0; 1] space
      float2 ratio_in_uv_space =
        ratio_of_aniso_rounded * float2(1.0/std_tex_size.u, 1.0/std_tex_size.v);

      // Get start and end positions of the anisotropic line
      float2 start_pos = input.tex_coords - aniso_line_direction * (ratio_in_uv_space / 2.0);
      float2 end_pos = start_pos + aniso_line_direction * ratio_in_uv_space;

      // Get the current sample position
      float2 sample_pos = lerp( start_pos, end_pos, aniso_idx / (ratio_of_aniso_rounded-1) );

      // Accumulate the sample
      sum += textures[input.tex_id].SampleLevel(
          immut_sampler, sample_pos, mip_lv, 0, residency_status );
    }
    return sum / aniso_idx;
  }  // if ( orig_ratio_of_aniso > 1.0 )
  else {
    float4 color;
    do {
      color = textures[input.tex_id].SampleLevel(
          immut_sampler, input.tex_coords, mip_lv, 0, residency_status );
      mip_lv += 1;
    } while ( !CheckAccessFullyMapped(residency_status) );
    return color;
  }
}
