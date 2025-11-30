#version 450

/*
 * cullMode = VK_CULL_MODE_BACK_BIT;
 * frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
 *
 * gl_VertexIndex = 0          (1,0)                        gl_VertexIndex = 2
 * . . . . . . . . . . . . . . . . . . . . . . . . . . . . ..
 * .                            .                        .
 * .                            .                     .
 * .                            .                  .
 * .                            .               .
 * .                            .            .
 * .                            .         .
 * .                            .      .
 * .                            .   .
 * . . . . . . . . . . . . . . . .
 * .                          .(1,1)
 * .                       .
 * .                    .
 * .                 .
 * .              .
 * .           .
 * .        .
 * .     .
 * .  .
 * .
 * gl_VertexIndex = 1
 */

layout (location = 0) out vec2 uv;

void main()
{
  uv = vec2(gl_VertexIndex & 2, (gl_VertexIndex << 1) & 2);
  gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);
}
