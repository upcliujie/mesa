struct PSInput {
  float4 pos : SV_Position;
};

PSInput VSMain(float4 position : POSITION) {
  PSInput output;
  // swizzle turns into a vec4 alu op
  output.pos = position.wzyx;
  return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
  return float4(mul(input.pos, input.pos), 1.0 + input.pos.x, 1.0, 1.0);
}
