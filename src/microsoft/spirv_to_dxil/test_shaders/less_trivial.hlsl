cbuffer Constants {
  float4x4 mvp;
  float4x4 mw;
  float4 light_dir;
  float4 light_col;
  float4 ambient_col;
}

Texture2D tex;
SamplerState samp;

struct VSInput {
  float4 pos : POSITION;
  float4 norm : NORMAL;
  float2 uv : TEXCOORD;
};

struct PSInput {
  float4 color : COLOR;
  float2 uv : TEXCOORD;
  float4 pos : SV_POSITION;
};

PSInput VSMain(VSInput input) {
  PSInput output;
  output.pos = mul(mvp, input.pos);
  output.uv = input.uv;
  output.color = ambient_col;
  float4 norm = normalize(mul(mw, input.norm));
  float diffuse = saturate(dot(norm, light_dir));
  output.color += light_col * diffuse;
  return output;
}

float4 PSMain(float4 color : COLOR, float2 uv : TEXCOORD) : SV_TARGET {
  return color * tex.Sample(samp, uv);
}
